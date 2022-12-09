#include "Scene.h"

#include <algorithm>
#include "GLTFHelpers.h"

Scene::Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, GLTFResources* resources)
	:resources(resources)
{
	std::unordered_map<int, int> modelNodeToSceneEntity; // use for animations and other GLTF constructs which refer to model node indices
	int sceneNodeIndex = 0;
	for (int modelNodeIndex : scene.nodes)
	{
		modelNodeToSceneEntity[sceneNodeIndex] = modelNodeIndex;
		sceneNodeIndex++;
	}

	for (const auto& node : model.nodes)
	{
		entities.emplace_back();
		Entity& entity = entities.back();
		entity.name = node.name;
		entity.transform = GetNodeTransform(node);
		std::transform(node.children.begin(), node.children.end(), std::back_inserter(entity.children),
			[&modelNodeToSceneEntity](int childIndex)
			{ 
				auto iter = modelNodeToSceneEntity.find(childIndex);
				assert(iter != modelNodeToSceneEntity.end());
				return iter->second;
			});

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

	for (int i = 0; i < entities.size(); i++)
	{
		SetEntityParents(entities[i], i, -1);
	}

	for (const auto& gltfAnimation : model.animations)
	{
		bool animationBelongsToScene = modelNodeToSceneEntity.contains(gltfAnimation.channels[0].target_node);

		if (animationBelongsToScene)
		{
			animations.emplace_back();
			Animation& animation = animations.back();
			
			// TODO: set animation duration to multiple of (1.0 / framesPerSecond), AKA secondsPerFrame
			double animationDurationSeconds = GetAnimationDurationSeconds(gltfAnimation, model);
			animation.durationSeconds = (float)animationDurationSeconds; 

			for (const auto& channel : gltfAnimation.channels)
			{
				auto sceneEntityIndexIter = modelNodeToSceneEntity.find(channel.target_node);
				assert(sceneEntityIndexIter != modelNodeToSceneEntity.end());
				Entity* sceneEntity = &entities[sceneEntityIndexIter->second];
				auto entityAnimationIter = std::find_if(animation.entityAnimations.begin(), animation.entityAnimations.end(),
					[sceneEntity](const Animation::EntityAnimation& entityAnimation)
					{
						return entityAnimation.entity == sceneEntity;
					});
				Animation::EntityAnimation* entityAnimation;
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
				assert(sampler.interpolation == "LINEAR" && "Only linear interpolation supported for now.");
				const auto& keyframeTimesAccessor = model.accessors[sampler.input];
				const auto& keyframeValuesAccessor = model.accessors[sampler.output];
				if (channel.target_path == "translation")
				{
					entityAnimation->translations = GetFixedRateTRSValues<glm::vec3>(keyframeTimesAccessor, keyframeValuesAccessor, model, animation.framesPerSecond, animationDurationSeconds);
				}
				else if (channel.target_path == "scale")
				{
					entityAnimation->scales = GetFixedRateTRSValues<glm::vec3>(keyframeTimesAccessor, keyframeValuesAccessor, model, animation.framesPerSecond, animationDurationSeconds);
				}
				else if (channel.target_path == "rotation")
				{
					entityAnimation->rotations = GetFixedRateTRSValues<glm::quat>(keyframeTimesAccessor, keyframeValuesAccessor, model, animation.framesPerSecond, animationDurationSeconds);
				}
				else
				{
					entityAnimation->weights = GetFixedRateWeightValues(keyframeTimesAccessor, keyframeValuesAccessor, model, animation.framesPerSecond, animationDurationSeconds);
				}
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
	
	if (animations.size() == 0) return;

	const auto& anim = animations[0];
	float animTime = std::fmod(time, anim.durationSeconds);
	float keyframe = animTime * anim.framesPerSecond;
	float a = std::floor(keyframe);
	float t = keyframe - a;
	assert(t >= 0.0f && t <= 1.0f);

	int aIndex = (int)a;
	int bIndex = aIndex + 1;

	for (const auto& entityAnim : anim.entityAnimations)
	{
		Entity* entity = entityAnim.entity;
		
		if (entityAnim.rotations.size() > 0)
		{
			const auto& a = entityAnim.rotations[aIndex];
			const auto& b = entityAnim.rotations[bIndex];
			entity->transform.rotation = glm::slerp(a, b, t);
		}

		if (entityAnim.translations.size() > 0)
		{
			const auto& a = entityAnim.translations[aIndex];
			const auto& b = entityAnim.translations[bIndex];
			entity->transform.translation = t * a + (1.0f - t) * b;
		}

		if (entityAnim.scales.size() > 0)
		{
			const auto& a = entityAnim.scales[aIndex];
			const auto& b = entityAnim.scales[bIndex];
			entity->transform.scale = t * a + (1.0f - t) * b;
		}

		if (entityAnim.weights.size() > 0)
		{
			const int numMorphTargets = entity->morphTargetWeights.size();
			for (int i = 0; i < numMorphTargets; i++)
			{
				float a = entityAnim.weights[aIndex * numMorphTargets + i];
				float b = entityAnim.weights[bIndex * numMorphTargets + i];
				entity->morphTargetWeights[i] = t * a + (1.0f - t) * b;
			}
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

void Scene::SetEntityParents(Entity& entity, int entityIndex, int parent)
{
	entity.parent = parent;

	for (int childIndex : entity.children)
	{
		SetEntityParents(entities[childIndex], childIndex, entityIndex);
	}
}
