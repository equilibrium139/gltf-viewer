#include "Scene.h"

#include <algorithm>
#include "GLTFHelpers.h"
#include <GLFW/glfw3.h>
#include "imgui.h"

// TODO: move rendering stuff to its own class, otherwise buffers will be needlessly duplicated for each scene

Scene::Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, int windowWidth, int windowHeight, GLuint fbo, 
	GLuint fullscreenQuadVAO,
	GLuint colorTexture,
	GLuint highlightTexture,
	GLuint depthStencilRBO,
	GLuint lightsUBO)
	:resources(model), fbo(fbo), fullscreenQuadVAO(fullscreenQuadVAO), colorTexture(colorTexture), highlightTexture(highlightTexture), depthStencilRBO(depthStencilRBO), texW(windowWidth), texH(windowHeight), lightsUBO(lightsUBO)
{
	assert(model.scenes.size() == 1); // for now

	int defaultEntityNameSuffix = 0;

	// used to set light's entityIdx later
	std::unordered_map<int, int> lightEntity;
	
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

		if (node.camera >= 0) 
		{
			entity.cameraIdx = node.camera;
		}

		auto lightsExtension = node.extensions.find("KHR_lights_punctual");
		if (lightsExtension != node.extensions.end())
		{
			int lightIdx = lightsExtension->second.Get("light").GetNumberAsInt();
			entity.lightIdx = lightIdx;
			lightEntity[lightIdx] = entities.size() - 1;
		}
	}

	globalTransforms.resize(entities.size());

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

	animationEnabled.resize(animations.size(), true);

	controllableCamera.name = "Controllable Camera";
	int defaultCameraNameSuffix = 0;
	for (const tinygltf::Camera& gltfCamera : model.cameras)
	{
		bool perspective = gltfCamera.type == "perspective";
		if (!perspective)
		{
			std::cout << "Warning: Orthographic cameras not currently supported\n";
		}
		cameras.emplace_back();
		Camera& camera = cameras.back();
		if (!gltfCamera.name.empty())
		{
			camera.name = gltfCamera.name;
		}
		else
		{
			camera.name = "Camera " + std::to_string(defaultCameraNameSuffix++);
		}
		// TODO: handle gltf camera aspect ratio
		if (perspective)
		{
			camera.zoom = glm::degrees(gltfCamera.perspective.yfov);
			camera.near = gltfCamera.perspective.znear;
			camera.far = gltfCamera.perspective.zfar;
		}
	}
	
	int lightIdx = 0;
	for (const tinygltf::Light& gltfLight : model.lights)
	{
		glm::vec3 color = gltfLight.color.size() == 3 ? glm::vec3((float)gltfLight.color[0], (float)gltfLight.color[1], (float)gltfLight.color[2]) : glm::vec3(1.0f);

		// TODO: implement this as bool indicating if light has infinite range?
		const float gltfLightRange = gltfLight.range != 0.0 ? (float)gltfLight.range : FLT_MAX;

		if (gltfLight.type == "point")
		{
			Light light{
				.type = Light::Point,
				.color = color,
				.intensity = (float)gltfLight.intensity,
				.range = gltfLightRange,
				.entityIdx = lightEntity[lightIdx]
			};
			lights.push_back(light);
		}
		else if (gltfLight.type == "spot")
		{
			Light light{
				.type = Light::Spot,
				.color = color,
				.intensity = (float)gltfLight.intensity,
				.range = gltfLightRange,
				.innerAngleCutoffDegrees = glm::degrees((float)gltfLight.spot.innerConeAngle),
				.outerAngleCutoffDegrees = glm::degrees((float)gltfLight.spot.outerConeAngle),
				.entityIdx = lightEntity[lightIdx]
			};
			lights.push_back(light);
		}
		else
		{
			assert(gltfLight.type == "directional");
			Light light{
				.type = Light::Directional,
				.color = color,
				.intensity = (float)gltfLight.intensity,
				.entityIdx = lightEntity[lightIdx]
			};
			lights.push_back(light);
		}
		lightIdx++;
	}

	// add default lights if scene doesn't have lights
	if (lights.size() == 0)
	{
		// TODO: sync entites and global transforms array in a cleaner way
		int entityIdx = entities.size();
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& pointLightEntity = entities.back();
		pointLightEntity.name = "DefaultPointLightEntity";
		pointLightEntity.transform.translation = glm::vec3(0.0f, 3.0f, 0.0f);
		pointLightEntity.transform.scale = glm::vec3(1.0f); // TODO: make lights independent of entity scale
		Light pointLight{
			.type = Light::Point, 
			.color = glm::vec3(1.0f, 0.0f, 0.0f), 
			.intensity = 50.0f,
			.range = 50.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(pointLight);

		entityIdx++;
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& spotLightEntity = entities.back();
		spotLightEntity.name = "DefaultSpotLightEntity";
		spotLightEntity.transform.translation = glm::vec3(0.0f, 0.5f, 5.0f);
		spotLightEntity.transform.rotation = glm::quat(glm::vec3(0.0f, glm::radians(180.0f), 0.0f));
		spotLightEntity.transform.scale = glm::vec3(1.0f);
		Light spotLight{
			.type = Light::Spot,
			.color = glm::vec3(0.0f, 1.0f, 0.0f),
			.intensity = 50.0f,
			.range = 50.0f,
			.innerAngleCutoffDegrees = 10.0f,
			.outerAngleCutoffDegrees = 50.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(spotLight);

		entityIdx++;
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& dirLightEntity = entities.back();
		dirLightEntity.name = "DefaultDirectionalLightEntity";
		dirLightEntity.transform.rotation = glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f));
		dirLightEntity.transform.scale = glm::vec3(1.0f);
		Light dirLight{
			.type = Light::Directional,
			.color = glm::vec3(0.0f, 0.0f, 1.0f),
			.intensity = 100.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(dirLight);
	}
}

void Scene::Render(int windowWidth, int windowHeight)
{
	UpdateGlobalTransforms();

	const auto view = currentCamera->GetViewMatrix();
	const float aspectRatio = windowHeight > 0 ? (float)windowWidth / (float)windowHeight : 1.0f;
	const auto projection = currentCamera->GetProjectionMatrix(aspectRatio);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, texW, texH);
	
	std::int32_t numLights[3] = { 0, 0, 0 };
	std::vector<PointLight> pointLights;
	std::vector<SpotLight> spotLights;
	std::vector<DirectionalLight> dirLights;
	for (const Light& light : lights)
	{
		assert(light.entityIdx >= 0);
		const glm::mat4& entityGlobalTransform = globalTransforms[light.entityIdx];
		glm::vec3 lightPosVS = view * glm::vec4(glm::vec3(entityGlobalTransform[3]), 1.0f);
		glm::vec3 lightDirVS = view * glm::vec4(glm::normalize(glm::vec3(entityGlobalTransform[2])), 0.0f);
		switch (light.type) 
		{
		case Light::Point:
			pointLights.emplace_back(light.color, lightPosVS, light.range, light.intensity);
			numLights[0]++;
			break;
		case Light::Spot:
			spotLights.emplace_back(light.color, lightPosVS, lightDirVS, light.range, light.innerAngleCutoffDegrees, light.outerAngleCutoffDegrees, light.intensity);
			numLights[1]++;
			break;
		case Light::Directional:
			dirLights.emplace_back(light.color, lightDirVS, light.intensity);
			numLights[2]++;
			break;
		}
	}
	
	assert(numLights[0] <= Shader::maxPointLights && numLights[1] <= Shader::maxSpotLights && numLights[2] <= Shader::maxDirLights);

	glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
	if (numLights[0] > 0) {
		glBufferSubData(GL_UNIFORM_BUFFER, 0, numLights[0] * sizeof(PointLight), pointLights.data());
	}
	if (numLights[1] > 0) {
		glBufferSubData(GL_UNIFORM_BUFFER, Shader::maxPointLights * sizeof(PointLight), numLights[1] * sizeof(SpotLight), spotLights.data());
	}
	if (numLights[2] > 0) {
		glBufferSubData(GL_UNIFORM_BUFFER, Shader::maxPointLights * sizeof(PointLight) + Shader::maxSpotLights * sizeof(SpotLight), numLights[2] * sizeof(DirectionalLight), dirLights.data());
	}
	glBufferSubData(GL_UNIFORM_BUFFER, Shader::maxPointLights * sizeof(PointLight) + Shader::maxSpotLights * sizeof(SpotLight) + Shader::maxDirLights * sizeof(DirectionalLight), sizeof(numLights), numLights);

	for (int i = 0; i < entities.size(); i++)
	{
		Entity& entity = entities[i];
		if (entity.meshIdx < 0)
		{
			continue;
		}
		bool highlight = selectedEntityIdx == i || IsParent(i, selectedEntityIdx);
		if (highlight)
		{
			glColorMaski(1, 0xFF, 0xFF, 0xFF, 0xFF);
		}
		else
		{
			glColorMaski(1, 0x00, 0x00, 0x00, 0x00);
		}
		const glm::mat4& globalTransform = globalTransforms[i];
		glm::mat4 modelView = view * globalTransform;
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

				// TODO: don't set unused textures
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

					if (material.normalTextureIdx >= 0)
					{
						glActiveTexture(GL_TEXTURE0 + textureUnit);
						glBindTexture(GL_TEXTURE_2D, resources.textures[material.normalTextureIdx].id);
						shader.SetInt("material.normalTexture", textureUnit);
						textureUnit++;

						// This uniform variable is only used if tangents (which are synonymous with normal mapping for now)
						// are provided
						if (HasFlag(submesh.flags, VertexAttribute::TANGENT))
						{
							shader.SetFloat("material.normalScale", material.normalScale);
						}
					}

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

			// TODO: find better place for calculating scene bbox/non-rendering stuff
			auto bboxWorldPoints = entityMesh.boundingBox.GetPoints();
			for (glm::vec3& point : bboxWorldPoints)
			{
				point = globalTransform * glm::vec4(point, 1.0f);
				sceneBoundingBox.minXYZ = glm::min(point, sceneBoundingBox.minXYZ);
				sceneBoundingBox.maxXYZ = glm::max(point, sceneBoundingBox.maxXYZ);
			}
		}

		if (entity.cameraIdx >= 0)
		{
			// We don't need to mess with yaw/pitch. Those are only used for changing camera via keyboard/mouse input. 
			// Entity cameras are not controllable by input; they only change when their entity's transform changes.
			Camera& camera = cameras[entity.cameraIdx];
			glm::vec4 globalTransformXAxisNormalized = glm::normalize(globalTransform[0]);
			glm::vec4 globalTransformYAxisNormalized = glm::normalize(globalTransform[1]);
			glm::vec4 globalTransformZAxisNormalized = glm::normalize(globalTransform[2]);
			camera.right = globalTransformXAxisNormalized;
			camera.up = globalTransformYAxisNormalized;
			camera.front = -globalTransformZAxisNormalized;
			camera.position = glm::vec3(globalTransform[3]);
		}
	}

	//RenderBoundingBox(sceneBoundingBox, projection * view);

	// Position controllable camera according to scene bounding box
	if (firstFrame)
	{
		firstFrame = false;
		glm::vec3 dims = sceneBoundingBox.maxXYZ - sceneBoundingBox.minXYZ;
		glm::vec3 center = sceneBoundingBox.GetCenter();
		glm::vec3 offsetFromCenter(0.0f);
		float maxDim = std::max({ dims.x, dims.y, dims.z });
		if (dims.x < dims.y)
		{
			if (dims.x < dims.z)
			{
				offsetFromCenter.x = maxDim;
			}
			else
			{
				offsetFromCenter.z = maxDim;
			}
		}
		else
		{
			if (dims.y < dims.z)
			{
				offsetFromCenter.y = maxDim;
			}
			else
			{
				offsetFromCenter.z = maxDim;
			}
		}
		controllableCamera.position = center + offsetFromCenter;
		controllableCamera.LookAt(center);
		auto bboxPoints = sceneBoundingBox.GetPoints();
		while (true)
		{
			glm::mat4 mvp = controllableCamera.GetProjectionMatrix(aspectRatio) * controllableCamera.GetViewMatrix();
			bool allPointsInCamView = true;
			for (const glm::vec3& point : bboxPoints)
			{
				glm::vec4 clipSpacePoint = mvp * glm::vec4(point, 1.0f);
				glm::vec3 ndcPoint(clipSpacePoint.x / clipSpacePoint.w, clipSpacePoint.y / clipSpacePoint.w, clipSpacePoint.z / clipSpacePoint.w);
				if (ndcPoint.x < -1.0f || ndcPoint.x > 1.0f ||
					ndcPoint.y < -1.0f || ndcPoint.y > 1.0f)
				{
					allPointsInCamView = false;
					break;
				}
			}
			if (allPointsInCamView)
			{
				break;
			}
			else
			{
				offsetFromCenter *= 1.5f;
				controllableCamera.position = center + offsetFromCenter;
			}
		}
		controllableCamera.movementSpeed = maxDim / 5.0f;
	}
}

void Scene::UpdateAndRender(const Input& input)
{
	if (currentCamera == &controllableCamera)
	{
		if (input.wPressed) currentCamera->ProcessKeyboard(CAM_FORWARD, input.deltaTime);
		if (input.aPressed) currentCamera->ProcessKeyboard(CAM_LEFT, input.deltaTime);
		if (input.sPressed) currentCamera->ProcessKeyboard(CAM_BACKWARD, input.deltaTime);
		if (input.dPressed) currentCamera->ProcessKeyboard(CAM_RIGHT, input.deltaTime);

		if (input.leftMousePressed) currentCamera->ProcessMouseMovement(input.mouseDeltaX, input.mouseDeltaY);
	}

	time += input.deltaTime;

	for (int i = 0; i < animations.size(); i++)
	{
		if (animationEnabled[i])
		{
			const auto& anim = animations[i];

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
	}

	RenderUI();
	Render(input.windowWidth, input.windowHeight);
}

void Scene::RenderUI()
{
	ImGui::Begin("Scene");

	for (int i = 0; i < entities.size(); i++)
	{
		if (entities[i].parent < 0)
		{
			RenderHierarchyUI(i);
		}
	}

	ImGui::End();
	
	if (selectedEntityIdx >= 0)
	{
		Entity& selectedEntity = entities[selectedEntityIdx];
		ImGui::Begin("Transform");

		ImGui::DragFloat3("Translation", &selectedEntity.transform.translation.x, 0.1f);
		ImGui::DragFloat4("Rotation(quat)", &selectedEntity.transform.rotation.x);
		ImGui::DragFloat3("Scale", &selectedEntity.transform.scale.x, 0.1f);

		ImGui::End();
	}

	if (!animations.empty())
	{
		ImGui::Begin("Animations");

		for (int i = 0; i < animations.size(); i++)
		{
			ImGui::Checkbox(animations[i].name.c_str(), reinterpret_cast<bool*>(&animationEnabled[i]));
		}

		ImGui::End();
	}

	if (!cameras.empty())
	{
		ImGui::Begin("Cameras");

		const char* currentCameraName = currentCamera->name.c_str();
		if (ImGui::BeginCombo("Camera", currentCameraName))
		{
			const bool isSelected = currentCamera == &controllableCamera;
			if (ImGui::Selectable(controllableCamera.name.c_str(), isSelected))
			{
				currentCamera = &controllableCamera;
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < cameras.size(); i++)
			{
				const bool isSelected = currentCamera == &cameras[i];
				if (ImGui::Selectable(cameras[i].name.c_str(), isSelected))
				{
					currentCamera = &cameras[i];
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImGui::End();
	}
}

void Scene::RenderHierarchyUI(int entityIdx)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if (entityIdx == selectedEntityIdx)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	Entity& entity = entities[entityIdx];
	if (ImGui::TreeNodeEx(entity.name.c_str(), flags))
	{
		if (ImGui::IsItemClicked())
		{
			selectedEntityIdx = entityIdx;
		}
		for (int childIdx : entity.children)
		{
			RenderHierarchyUI(childIdx);
		}

		ImGui::TreePop();
	}
	else
	{
		if (ImGui::IsItemClicked())
		{
			selectedEntityIdx = entityIdx;
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

void Scene::UpdateGlobalTransforms()
{
	for (int i = 0; i < entities.size(); i++)
	{
		Entity& entity = entities[i];
		if (entity.parent < 0) {
			UpdateGlobalTransforms(i, glm::mat4(1.0f));
		}
	}
}

void Scene::UpdateGlobalTransforms(int entityIdx, const glm::mat4& parentTransform)
{
	glm::mat4& globalTransform = globalTransforms[entityIdx];
	const Entity& entity = entities[entityIdx];
	globalTransform = parentTransform * entity.transform.GetMatrix();
	for (int child : entity.children)
	{
		UpdateGlobalTransforms(child, globalTransform);
	}
}

bool Scene::IsParent(int entityChild, int entityParent)
{
	if (entities[entityChild].parent < 0)
	{
		return false;
	}

	return IsParent(entities[entityChild].parent, entityParent);
}
