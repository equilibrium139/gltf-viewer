#include "Scene.h"

#include <algorithm>
#include "GLTFHelpers.h"
#include "imgui.h"

Scene::Scene(const tinygltf::Scene& scene, const tinygltf::Model& model)
	:resources(model)
{
	assert(model.scenes.size() == 1); // for now
	// TODO: clean this up
	GLfloat vertices[] = {
		// Outline vertices
		-0.5f, -0.5f, -0.5f,  // Vertex 0
		 0.5f, -0.5f, -0.5f,  // Vertex 1
		 0.5f, -0.5f,  0.5f,  // Vertex 2
		-0.5f, -0.5f,  0.5f,  // Vertex 3
		-0.5f,  0.5f, -0.5f,  // Vertex 4
		 0.5f,  0.5f, -0.5f,  // Vertex 5
		 0.5f,  0.5f,  0.5f,  // Vertex 6
		-0.5f,  0.5f,  0.5f   // Vertex 7
	};

	GLushort indices[] = {
		// Outline edges
		0, 1, 1, 2, 2, 3, 3, 0,  // Bottom face
		4, 5, 5, 6, 6, 7, 7, 4,  // Top face
		0, 4, 1, 5, 2, 6, 3, 7   // Vertical edges
	};

	glGenVertexArrays(1, &boundingBoxVAO);
	glBindVertexArray(boundingBoxVAO);

	GLuint boundingBoxVBO;
	glGenBuffers(1, &boundingBoxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boundingBoxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);

	GLuint boundingBoxIBO;
	glGenBuffers(1, &boundingBoxIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boundingBoxIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	camera.position.z = 5.0f;

	int defaultEntityNameSuffix = 0;
	
	// Nodes
	for (const auto& node : model.nodes)
	{
		entities.emplace_back();
		Entity& entity = entities.back();
		entity.name = node.name;
		if (entity.name.empty())
		{
			entity.name = "Entity" + std::to_string(defaultEntityNameSuffix);
			defaultEntityNameSuffix++;
		}
		entity.transform = GetNodeTransform(node);
		entity.children = node.children;

		// TODO: add support for cameras
		if (node.mesh >= 0)
		{
			entity.meshIdx = node.mesh;
			Mesh& entityMesh = resources.meshes[entity.meshIdx];
			// TODO: add support for more than 2 morph targets
			bool hasMorphTargets = entityMesh.HasMorphTargets();
			if (hasMorphTargets)
			{
				entity.morphTargetWeights.resize(2);
			}
		}
	}

	// Set entity parent
	for (int i = 0; i < entities.size(); i++)
	{
		const auto& entity = entities[i];
		for (int j = 0; j < entity.children.size(); j++)
		{
			auto& child = entities[entity.children[j]];
			child.parent = i;
		}
	}

	// Skeletons
	for (const tinygltf::Skin& skin : model.skins)
	{
		skeletons.emplace_back();
		auto& skeleton = skeletons.back();
		int numJoints = skin.joints.size();

		std::vector<std::uint8_t> inverseBindMatricesBytes = GetAccessorBytes(model.accessors[skin.inverseBindMatrices], model);
		assert(inverseBindMatricesBytes.size() == sizeof(glm::mat4) * numJoints);
		std::span<glm::mat4> inverseBindMatrices((glm::mat4*)(inverseBindMatricesBytes.data()), numJoints);

		for (int i = 0; i < numJoints; i++)
		{
			skeleton.joints.emplace_back();
			auto& joint = skeleton.joints.back();
			joint.localToJoint = glm::mat4x3(inverseBindMatrices[i]);
			joint.entityIndex = skin.joints[i];
			int parentEntityIndex = entities[joint.entityIndex].parent;
			if (parentEntityIndex < 0)
			{
				joint.parent = -1;
			}
			else
			{
				joint.parent = 0;
				while (joint.parent < skeleton.joints.size() && skeleton.joints[joint.parent].entityIndex != parentEntityIndex)
				{
					joint.parent++;
				}
				if (joint.parent >= skeleton.joints.size()) joint.parent = -1;
			}
		}
	}

	// After initializing skeletons, set entity skeletons
	for (int i = 0; i < entities.size(); i++)
	{
		if (model.nodes[i].skin >= 0)
		{
			entities[i].skeletonIdx = model.nodes[i].skin;
		}
	}

	int defaultAnimationNameSuffix = 0;
	// Animations
	for (const auto& gltfAnimation : model.animations)
	{
		animations.emplace_back();
		Animation& animation = animations.back();

		double animationDurationSeconds = GetAnimationDurationSeconds(gltfAnimation, model);
		animation.durationSeconds = (float)animationDurationSeconds;

		if (gltfAnimation.name.empty())
		{
			animation.name = "Anim " + std::to_string(defaultAnimationNameSuffix++);
		}
		else
		{
			animation.name = gltfAnimation.name;
		}
			
		for (const auto& channel : gltfAnimation.channels)
		{
			Entity* sceneEntity = &entities[channel.target_node];
			
			// Entity might already have another animated channel in this animation, check if so
			auto entityAnimationIter = std::find_if(animation.entityAnimations.begin(), animation.entityAnimations.end(),
				[&channel](const EntityAnimation& entityAnimation)
				{
					return entityAnimation.entityIdx == channel.target_node;
				});
			EntityAnimation* entityAnimation;
			if (entityAnimationIter != animation.entityAnimations.end())
			{
				entityAnimation = &(*entityAnimationIter);
			}
			else
			{
				animation.entityAnimations.emplace_back();
				entityAnimation = &animation.entityAnimations.back();
			}
				
			entityAnimation->entityIdx = channel.target_node;

			const auto& sampler = gltfAnimation.samplers[channel.sampler];
			InterpolationType method = InterpolationType::LINEAR;
			if (sampler.interpolation == "STEP") method = InterpolationType::STEP;
			else if (sampler.interpolation == "CUBICSPLINE") method = InterpolationType::CUBICSPLINE;
			assert(channel.target_path != "weights" || method == InterpolationType::LINEAR && "Non-linear interpolation not supported for weights currently");
			const auto& keyframeTimesAccessor = model.accessors[sampler.input];
			const auto& keyframeValuesAccessor = model.accessors[sampler.output];
			if (channel.target_path == "translation")
			{
				auto translationsBytes = GetAccessorBytes(keyframeValuesAccessor, model);
				std::span<glm::vec3> translations((glm::vec3*)translationsBytes.data(), keyframeValuesAccessor.count);

				auto timesBytes = GetAccessorBytes(keyframeTimesAccessor, model);
				std::span<float> times((float*)timesBytes.data(), keyframeTimesAccessor.count);

				entityAnimation->translations.values.assign(translations.begin(), translations.end());
				entityAnimation->translations.times.assign(times.begin(), times.end());
				entityAnimation->translations.method = method;
			}
			else if (channel.target_path == "scale")
			{
				auto scalesBytes = GetAccessorBytes(keyframeValuesAccessor, model);
				std::span<glm::vec3> scales((glm::vec3*)scalesBytes.data(), keyframeValuesAccessor.count);

				auto timesBytes = GetAccessorBytes(keyframeTimesAccessor, model);
				std::span<float> times((float*)timesBytes.data(), keyframeTimesAccessor.count);

				entityAnimation->scales.values.assign(scales.begin(), scales.end());
				entityAnimation->scales.times.assign(times.begin(), times.end());
				entityAnimation->scales.method = method;
			}
			else if (channel.target_path == "rotation")
			{
				auto rotationsBytes = GetAccessorBytes(keyframeValuesAccessor, model);
				std::span<glm::quat> rotations((glm::quat*)rotationsBytes.data(), keyframeValuesAccessor.count);

				auto timesBytes = GetAccessorBytes(keyframeTimesAccessor, model);
				std::span<float> times((float*)timesBytes.data(), keyframeTimesAccessor.count);

				entityAnimation->rotations.values.assign(rotations.begin(), rotations.end());
				entityAnimation->rotations.times.assign(times.begin(), times.end());
				entityAnimation->rotations.method = method;
			}
			else
			{
				auto weightsBytes = GetAccessorBytes(keyframeValuesAccessor, model);
				std::span<float> weights((float*)weightsBytes.data(), keyframeValuesAccessor.count);

				auto timesBytes = GetAccessorBytes(keyframeTimesAccessor, model);
				std::span<float> times((float*)timesBytes.data(), keyframeTimesAccessor.count);

				entityAnimation->weights.values.assign(weights.begin(), weights.end());
				entityAnimation->weights.times.assign(times.begin(), times.end());
				entityAnimation->weights.method = method;
			}
		}
	}
}

void Scene::Render(float aspectRatio)
{
	const auto view = camera.GetViewMatrix();
	const auto projection = camera.GetProjectionMatrix(aspectRatio);
	for (const Entity& entity : entities)
	{
		if (entity.parent < 0) RenderEntity(entity, glm::mat4(1.0f), view, projection);
	}

	RenderBoundingBox(sceneBoundingBox, projection * view);
}

void Scene::UpdateAndRender(const Input& input)
{
	if (input.wPressed) camera.ProcessKeyboard(CAM_FORWARD, input.deltaTime);
	if (input.aPressed) camera.ProcessKeyboard(CAM_LEFT, input.deltaTime);
	if (input.sPressed) camera.ProcessKeyboard(CAM_BACKWARD, input.deltaTime);
	if (input.dPressed) camera.ProcessKeyboard(CAM_RIGHT, input.deltaTime);

	if (input.leftMousePressed) camera.ProcessMouseMovement(input.mouseDeltaX, input.mouseDeltaY);

	time += input.deltaTime;

	if (currentAnimationIdx < animations.size())
	{
		const auto& anim = animations[currentAnimationIdx];

		float normalizedTime = std::fmod(time, anim.durationSeconds);

		for (const auto& entityAnim : anim.entityAnimations)
		{
			Entity& entity = entities[entityAnim.entityIdx];

			if (entityAnim.translations.values.size() > 0) entity.transform.translation = SampleAt(entityAnim.translations, normalizedTime);
			if (entityAnim.scales.values.size() > 0) entity.transform.scale = SampleAt(entityAnim.scales, normalizedTime);
			if (entityAnim.rotations.values.size() > 0) entity.transform.rotation = SampleAt(entityAnim.rotations, normalizedTime);
			if (entityAnim.weights.values.size() > 0) entity.morphTargetWeights = SampleWeightsAt(entityAnim.weights, normalizedTime);
		}
	}

	RenderUI();

	if (input.windowWidth != 0 && input.windowHeight != 0)
	{
		const float aspectRatio = (float)input.windowWidth / (float)input.windowHeight;
		Render(aspectRatio);
	}
}

void Scene::RenderEntity(const Entity& entity, const glm::mat4& parentTransform, const glm::mat4& view, const glm::mat4& projection)
{
	glm::mat4 globalTransform = parentTransform * entity.transform.GetMatrix();
	glm::mat4 modelView = view * globalTransform;
	if (entity.meshIdx >= 0)
	{
		Mesh& entityMesh = resources.meshes[entity.meshIdx];
		for (Submesh& submesh : entityMesh.submeshes)
		{
			glBindVertexArray(submesh.VAO);
			Shader& shader = resources.GetOrCreateShader(submesh.flags, submesh.flatShading);
			shader.use();
			shader.SetMat4("modelView", glm::value_ptr(modelView));
			shader.SetMat4("projection", glm::value_ptr(projection));
			bool hasNormals = HasFlag(submesh.flags, VertexAttribute::NORMAL);
			if (hasNormals)
			{
				glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelView)));
				shader.SetMat3("normalMatrixVS", glm::value_ptr(normalMatrix));
				shader.SetVec3("pointLight.positionVS", glm::vec3(0.0f, 0.0f, 0.0f));
				shader.SetVec3("pointLight.color", glm::vec3(0.5f, 0.5f, 0.5f));
			}
			if (entity.skeletonIdx >= 0)
			{
				auto skinningMatrices = ComputeSkinningMatrices(skeletons[entity.skeletonIdx], entities);
				shader.SetMat4("skinningMatrices", glm::value_ptr(skinningMatrices.front()), (int)skinningMatrices.size());
			}
			bool hasMorphTargets = HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION);
			if (hasMorphTargets)
			{
				shader.SetFloat("morph1Weight", entity.morphTargetWeights[0]);
				shader.SetFloat("morph2Weight", entity.morphTargetWeights[1]);
			}
			if (submesh.materialIndex >= 0)
			{
				const PBRMaterial& material = resources.materials[submesh.materialIndex];
				shader.SetVec4("material.baseColorFactor", material.baseColorFactor);
				shader.SetFloat("material.metallicFactor", material.metallicFactor);
				shader.SetFloat("material.roughnessFactor", material.roughnessFactor);
				shader.SetFloat("material.occlusionStrength", material.occlusionStrength);

				bool hasTextureCoords = HasFlag(submesh.flags, VertexAttribute::TEXCOORD);

				if (hasTextureCoords)
				{
					int textureUnit = 0;
					glActiveTexture(GL_TEXTURE0 + textureUnit);
					glBindTexture(GL_TEXTURE_2D, resources.textures[material.baseColorTextureIdx].id);
					shader.SetInt("material.baseColorTexture", textureUnit);
					textureUnit++;

					glActiveTexture(GL_TEXTURE0 + textureUnit);
					glBindTexture(GL_TEXTURE_2D, resources.textures[material.metallicRoughnessTextureIdx].id);
					shader.SetInt("material.metallicRoughnessTexture", textureUnit);
					textureUnit++;

					// TODO: add normal mapping
					//if (material.normalTextureIdx >= 0)
					//{
					//	glActiveTexture(GL_TEXTURE0 + textureUnit);
					//	glBindTexture(GL_TEXTURE_2D, resources.textures[material.normalTextureIdx].id);
					//	shader.SetInt("material.baseColorTexture", textureUnit);
					//	textureUnit++;
					//}

					glActiveTexture(GL_TEXTURE0 + textureUnit);
					glBindTexture(GL_TEXTURE_2D, resources.textures[material.occlusionTextureIdx].id);
					shader.SetInt("material.occlusionTexture", textureUnit);
					textureUnit++;
				}
			}
			if (submesh.hasIndexBuffer)
			{
				glDrawElements(GL_TRIANGLES, submesh.countVerticesOrIndices, GL_UNSIGNED_INT, nullptr);
			}
			else
			{
				glDrawArrays(GL_TRIANGLES, 0, submesh.countVerticesOrIndices);
			}
		}

		RenderBoundingBox(entityMesh.boundingBox, projection * modelView);

		auto bboxWorldPoints = entityMesh.boundingBox.GetPoints();
		auto globalTransform3x3 = glm::mat3(globalTransform);
		for (glm::vec3& point : bboxWorldPoints)
		{
			point = globalTransform3x3 * point;
			sceneBoundingBox.minXYZ = glm::min(point, sceneBoundingBox.minXYZ);
			sceneBoundingBox.maxXYZ = glm::max(point, sceneBoundingBox.maxXYZ);
		}
	}

	for (int childIndex : entity.children)
	{
		RenderEntity(entities[childIndex], globalTransform, view, projection);
	}
}

void Scene::RenderUI()
{
	ImGui::Begin("Scene");

	for (int i = 0; i < entities.size(); i++)
	{
		if (entities[i].parent < 0)
		{
			RenderHierarchyUI(entities[i]);
		}
	}

	ImGui::End();

	Entity* selectedEntity = nullptr;
	for (Entity& entity : entities)
	{
		if (entity.name == selectedEntityName)
		{
			selectedEntity = &entity;
			break;
		}
	}
	if (selectedEntity != nullptr)
	{
		ImGui::Begin("Transform");

		ImGui::DragFloat3("Translation", &selectedEntity->transform.translation.x, 0.1f);
		ImGui::DragFloat4("Rotation(quat)", &selectedEntity->transform.rotation.x);
		ImGui::DragFloat3("Scale", &selectedEntity->transform.scale.x, 0.1f);

		ImGui::End();
	}

	if (!animations.empty())
	{
		ImGui::Begin("Animations");

		const char* currentAnimName = animations[currentAnimationIdx].name.c_str();
		if (ImGui::BeginCombo("Animation", currentAnimName))
		{
			for (int i = 0; i < animations.size(); i++)
			{
				const bool is_selected = (currentAnimationIdx == i);
				if (ImGui::Selectable(animations[i].name.c_str(), is_selected))
				{
					currentAnimationIdx = i;
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImGui::End();
	}
}

void Scene::RenderHierarchyUI(const Entity& entity)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if (entity.name == selectedEntityName)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (ImGui::TreeNodeEx(entity.name.c_str(), flags))
	{
		if (ImGui::IsItemClicked())
		{
			selectedEntityName = entity.name;
		}
		for (int childIndex : entity.children)
		{
			const Entity& child = entities[childIndex];

			RenderHierarchyUI(child);
		}

		ImGui::TreePop();
	}
	else
	{
		if (ImGui::IsItemClicked())
		{
			selectedEntityName = entity.name;
		}
	}
}

void Scene::RenderBoundingBox(const BBox& bbox, const glm::mat4& mvp)
{
	glm::vec3 dimensions = bbox.maxXYZ - bbox.minXYZ;
	glm::vec3 midpoint = bbox.minXYZ + dimensions * 0.5f;
	glm::mat4 cubeToBBox = glm::identity<glm::mat4>();
	cubeToBBox = glm::translate(cubeToBBox, midpoint);
	cubeToBBox = glm::scale(cubeToBBox, dimensions);
	glm::mat4 mat = mvp * cubeToBBox;
	glBindVertexArray(boundingBoxVAO);
	boundingBoxShader.use();
	boundingBoxShader.SetMat4("mvp", glm::value_ptr(mat));
	glDrawElements(GL_LINES, 24, GL_UNSIGNED_SHORT, 0);
}
