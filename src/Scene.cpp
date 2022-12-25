#include "Scene.h"

#include <algorithm>
#include "GLTFHelpers.h"

Scene::Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, GLTFResources* resources)
	:resources(resources)
{
	assert(model.scenes.size() == 1); // for now

	// Nodes
	for (const auto& node : model.nodes)
	{
		entities.emplace_back();
		Entity& entity = entities.back();
		entity.name = node.name;
		entity.transform = GetNodeTransform(node);
		entity.children = node.children;

		// TODO: add support for cameras
		if (node.mesh >= 0)
		{
			entity.mesh = &resources->meshes[node.mesh];
			// TODO: add support for more than 2 morph targets
			bool hasMorphTargets = HasFlag(entity.mesh->flags, VertexAttribute::MORPH_TARGET0_POSITION);
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
			entities[i].skeleton = &skeletons[model.nodes[i].skin];
		}
	}

	// Animations
	for (const auto& gltfAnimation : model.animations)
	{
		animations.emplace_back();
		Animation& animation = animations.back();

		double animationDurationSeconds = GetAnimationDurationSeconds(gltfAnimation, model);
		animation.durationSeconds = (float)animationDurationSeconds;
			
		for (const auto& channel : gltfAnimation.channels)
		{
			Entity* sceneEntity = &entities[channel.target_node];
			
			// Entity might already have another animated channel in this animation, check if so
			auto entityAnimationIter = std::find_if(animation.entityAnimations.begin(), animation.entityAnimations.end(),
				[sceneEntity](const EntityAnimation& entityAnimation)
				{
					return entityAnimation.entity == sceneEntity;
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
				
			entityAnimation->entity = sceneEntity;

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
		RenderEntity(entity, glm::mat4(1.0f), view, projection);
	}
}

void Scene::Update(float dt)
{
	time += dt;
	
	for (const auto& anim : animations)
	{
		float normalizedTime = std::fmod(time, anim.durationSeconds);

		for (const auto& entityAnim : anim.entityAnimations)
		{
			Entity* entity = entityAnim.entity;

			if (entityAnim.translations.values.size() > 0) entity->transform.translation = SampleAt(entityAnim.translations, normalizedTime);
			if (entityAnim.scales.values.size() > 0) entity->transform.scale = SampleAt(entityAnim.scales, normalizedTime);
			if (entityAnim.rotations.values.size() > 0) entity->transform.rotation = SampleAt(entityAnim.rotations, normalizedTime);
			if (entityAnim.weights.values.size() > 0) entity->morphTargetWeights = SampleWeightsAt(entityAnim.weights, normalizedTime);
		}
	}
}

void Scene::RenderEntity(const Entity& entity, const glm::mat4& parentTransform, const glm::mat4& view, const glm::mat4& projection)
{
	glm::mat4 globalTransform = parentTransform * entity.transform.GetMatrix();
	glm::mat4 modelView = view * globalTransform;

	if (entity.mesh != nullptr)
	{
		glBindVertexArray(entity.mesh->VAO);
		Shader& shader = resources->GetMeshShader(*entity.mesh); 
		shader.use();
		shader.SetMat4("modelView", glm::value_ptr(modelView));
		shader.SetMat4("projection", glm::value_ptr(projection));
		bool hasNormals = HasFlag(entity.mesh->flags, VertexAttribute::NORMAL);
		if (hasNormals)
		{
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelView)));
			shader.SetMat3("normalMatrixVS", glm::value_ptr(normalMatrix));
			shader.SetVec3("pointLight.positionVS", glm::vec3(0.0f, 0.0f, 0.0f));
			shader.SetVec3("pointLight.color", glm::vec3(0.5f, 0.5f, 0.5f));
		}
		if (entity.skeleton != nullptr)
		{
			auto skinningMatrices = ComputeSkinningMatrices(*entity.skeleton, entities);
			shader.SetMat4("skinningMatrices", glm::value_ptr(skinningMatrices.front()), (int)skinningMatrices.size());
		}
		bool hasMorphTargets = HasFlag(entity.mesh->flags, VertexAttribute::MORPH_TARGET0_POSITION);
		if (hasMorphTargets)
		{
			shader.SetFloat("morph1Weight", entity.morphTargetWeights[0]);
			shader.SetFloat("morph2Weight", entity.morphTargetWeights[1]);
		}
		if (entity.mesh->hasIndexBuffer)
		{
			std::uint32_t offset = 0;
			for (std::uint32_t indexCount : entity.mesh->submeshCountVerticesOrIndices)
			{
				glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (const void*)offset);
				offset += indexCount;
			}
		}
		else
		{
			std::uint32_t offset = 0;
			for (std::uint32_t vertexCount : entity.mesh->submeshCountVerticesOrIndices)
			{
				glDrawArrays(GL_TRIANGLES, offset, vertexCount);
			}
		}
	}

	for (int childIndex : entity.children)
	{
		RenderEntity(entities[childIndex], globalTransform, view, projection);
	}
}