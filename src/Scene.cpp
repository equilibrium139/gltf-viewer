#include "Scene.h"

#include <algorithm>
#include "GLTFHelpers.h"
#include <GLFW/glfw3.h>
#include "imgui.h"

// TODO: move rendering stuff to its own class, otherwise buffers will be needlessly duplicated for each scene

Scene::Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, int fbW, int fbH, GLuint fbo,
	GLuint fullscreenQuadVAO,
	GLuint colorTexture,
	GLuint highlightFBO,
	GLuint depthStencilRBO,
	GLuint lightsUBO,
	GLuint skyboxVAO,
	GLuint environmentMap,
	GLuint irradianceMap, 
	GLuint prefilterMap,
	GLuint brdfLUT)
	:resources(model), fbo(fbo), fullscreenQuadVAO(fullscreenQuadVAO), colorTexture(colorTexture), highlightFBO(highlightFBO), depthStencilRBO(depthStencilRBO), fbW(fbW), fbH(fbH), lightsUBO(lightsUBO), skyboxVAO(skyboxVAO), environmentMap(environmentMap), irradianceMap(irradianceMap), prefilterMap(prefilterMap),
	 brdfLUT(brdfLUT)
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
		if (perspective)
		{
			camera.zoom = glm::degrees(gltfCamera.perspective.yfov);
			camera.near = gltfCamera.perspective.znear;
			camera.far = gltfCamera.perspective.zfar;
			if (gltfCamera.perspective.aspectRatio > 0.0f)
			{
				camera.aspectRatio = gltfCamera.perspective.aspectRatio;
			}
			// TODO: for now using default aspect ratio which happens to match viewport aspect ratio
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
	if (lights.empty())
	{
		// TODO: sync entites and global transforms array in a cleaner way
		int entityIdx = entities.size();
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& pointLightEntity = entities.back();
		pointLightEntity.name = "DefaultPointLightEntity";
		pointLightEntity.transform.translation = glm::vec3(0.0f, 0.25f, 0.0f);
		pointLightEntity.transform.scale = glm::vec3(1.0f);
		Light pointLight{
			.type = Light::Point, 
			.color = glm::vec3(0.7f, 0.7f, 0.7f),
			.intensity = 0.0f,
			.range = 3.0f,
			.depthmapFarPlane = 5.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(pointLight);
		pointLightEntity.lightIdx = 0;

		entityIdx++;
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& spotLightEntity = entities.back();
		spotLightEntity.name = "DefaultSpotLightEntity";
		spotLightEntity.transform.translation = glm::vec3(0.0f, -3.36f, 0.7f);
		spotLightEntity.transform.rotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
		spotLightEntity.transform.scale = glm::vec3(1.0f);
		Light spotLight{
			.type = Light::Spot,
			.color = glm::vec3(0.7f, 0.7f, 0.7f),
			.intensity = 0.0f,
			.range = 25.0f,
			.innerAngleCutoffDegrees = 1.0f,
			.outerAngleCutoffDegrees = 2.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(spotLight);
		spotLightEntity.lightIdx = 1;

		entityIdx++;
		entities.emplace_back();
		globalTransforms.emplace_back();
		Entity& dirLightEntity = entities.back();
		dirLightEntity.name = "DefaultDirectionalLightEntity";
		dirLightEntity.transform.translation = glm::vec3(0.0f, 0.3f, 0.0f); // TODO: replace magic numbers
		dirLightEntity.transform.rotation = glm::quat(glm::vec3(glm::radians(126.0f), 0.0f, 0.0f));
		dirLightEntity.transform.scale = glm::vec3(1.0f); // TODO: make light independent of scale
		Light dirLight{
			.type = Light::Directional,
			.color = glm::vec3(0.7f, 0.7f, 0.7f),
			.intensity = 0.0f,
			.depthmapFarPlane = 10.0f,
			.entityIdx = entityIdx
		};
		lights.push_back(dirLight);
		dirLightEntity.lightIdx = 2;
	}

	depthMapFBOs.resize(lights.size());
	depthMaps.resize(lights.size());
	if (lights.size() > 0)
	{
		glGenFramebuffers(lights.size(), &depthMapFBOs.front());
		glGenTextures(lights.size(), &depthMaps.front());
	}

	for (int i = 0; i < lights.size(); i++)
	{
		GenerateShadowMap(i);
	}

	// TODO: put this somewhere else
	std::vector<glm::vec3> circleVertices(numCircleVertices);
	float angle = 0.0f;
	float angleDelta = glm::radians(360.0f / numCircleVertices);
	for (int i = 0; i < numCircleVertices; i++)
	{
		circleVertices[i].x = std::cos(angle);
		circleVertices[i].y = std::sin(angle);
		circleVertices[i].z = 0.0f;
		angle += angleDelta;
	}

	glGenVertexArrays(1, &circleVAO);
	glBindVertexArray(circleVAO);
	
	GLuint circleVBO;
	glGenBuffers(1, &circleVBO);
	glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(circleVertices[0]) * circleVertices.size(), &circleVertices.front(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);


	glm::vec3 lineVertices[2] = { glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
	glGenVertexArrays(1, &lineVAO);
	glBindVertexArray(lineVAO);
	
	GLuint lineVBO;
	glGenBuffers(1, &lineVBO);
	glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
	glBufferData(GL_ARRAY_BUFFER, 24, &lineVertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glGenVertexArrays(1, &frustumVAO);
	glBindVertexArray(frustumVAO);

	glGenBuffers(1, &frustumVBO);
	glBindBuffer(GL_ARRAY_BUFFER, frustumVBO);
	// 24 vertices for 12 lines, 4 on the near plane, 4 on the far plane, and 4 connecting the 2
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 24, nullptr, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
}

// TODO: figure out camera aspect ratio situation and potentially get rid of these parameters
void Scene::Render(int windowWidth, int windowHeight)
{
	const auto view = currentCamera->GetViewMatrix();
	const auto viewToWorld = glm::inverse(view);
	RenderShadowMaps();

	//const float aspectRatio = windowHeight > 0 ? (float)windowWidth / (float)windowHeight : 1.0f;
	const auto projection = currentCamera->GetProjectionMatrix();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, fbW, fbH);
	
	std::int32_t numLights[3] = { 0, 0, 0 };
	std::vector<PointLight> pointLights;
	std::vector<SpotLight> spotLights;
	std::vector<DirectionalLight> dirLights;
	// For now assuming all lights have shadow maps so lights[i] has depth map depthMaps[i]
	for (int i = 0; i < lights.size(); i++)
	{
		const Light& light = lights[i];
		assert(light.entityIdx >= 0);
		const glm::mat4& entityGlobalTransform = globalTransforms[light.entityIdx];
		glm::vec3 lightPosVS = view * glm::vec4(glm::vec3(entityGlobalTransform[3]), 1.0f);
		glm::vec3 lightDirVS = view * glm::vec4(glm::normalize(glm::vec3(entityGlobalTransform[2])), 0.0f);
		switch (light.type) 
		{
		case Light::Point:
			pointLights.emplace_back(light.color, lightPosVS, light.range, light.intensity, light.depthmapNearPlane, light.depthmapFarPlane, light.shadowMappingBias);
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
		const glm::mat4& globalTransform = globalTransforms[i];
		glm::mat4 modelView = view * globalTransform;
		Mesh& entityMesh = resources.meshes[entity.meshIdx];
		for (Submesh& submesh : entityMesh.submeshes)
		{
			Shader& shader = resources.GetOrCreateShader(submesh.flags, submesh.flatShading);
			shader.use();
			shader.SetMat4("world", glm::value_ptr(globalTransform));
			shader.SetMat4("view", glm::value_ptr(view));
			shader.SetMat4("viewToWorld", glm::value_ptr(viewToWorld));
			shader.SetMat4("projection", glm::value_ptr(projection));
			bool hasNormals = HasFlag(submesh.flags, VertexAttribute::NORMAL);
			int textureUnit = 0;
			if (hasNormals || submesh.flatShading)
			{
				if (hasNormals)
				{
					glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelView)));
					shader.SetMat3("normalMatrixVS", glm::value_ptr(normalMatrix));
				}

				glActiveTexture(GL_TEXTURE0 + textureUnit);
				glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
				shader.SetInt("irradianceMap", textureUnit);
				textureUnit++;

				glActiveTexture(GL_TEXTURE0 + textureUnit);
				glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
				shader.SetInt("prefilterMap", textureUnit);
				textureUnit++;

				glActiveTexture(GL_TEXTURE0 + textureUnit);
				glBindTexture(GL_TEXTURE_2D, brdfLUT);
				shader.SetInt("brdfLUT", textureUnit);
				textureUnit++;

				int numCubemaps = 0;
				int num2Dmaps = 0;
				int depthCubemapSamplers[Shader::maxPointLights];
				int depthMapSamplers[Shader::maxSpotLights + Shader::maxDirLights];
				std::fill_n(&depthMapSamplers[0], Shader::maxSpotLights + Shader::maxDirLights, -1);
				int spotLightIdx = 0;
				int dirLightIdx = 0;
				for (int lightIdx = 0; lightIdx < lights.size(); lightIdx++)
				{
					const Light& light = lights[lightIdx];
					if (light.type == Light::Point)
					{
						glActiveTexture(GL_TEXTURE0 + textureUnit);
						glBindTexture(GL_TEXTURE_CUBE_MAP, depthMaps[lightIdx]);
						depthCubemapSamplers[numCubemaps] = textureUnit;
						textureUnit++;
						numCubemaps++;
					}
					else
					{
						glm::mat4 translationWithBias = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f - light.shadowMappingBias));
						glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
						glm::mat4 worldToShadowMapUV = translationWithBias * scale * light.lightProjection;

						glActiveTexture(GL_TEXTURE0 + textureUnit);	
						glBindTexture(GL_TEXTURE_2D, depthMaps[lightIdx]);

						int uniformIdx = light.type == Light::Spot ? spotLightIdx++ : Shader::maxSpotLights + dirLightIdx++;
						std::string worldToShadowMapUniformName = "worldToShadowMapUVSpace[" + std::to_string(uniformIdx) + "]";
						shader.SetMat4(worldToShadowMapUniformName.c_str(), glm::value_ptr(worldToShadowMapUV), 1); // TODO: set them all at once
						depthMapSamplers[uniformIdx] = textureUnit;

						textureUnit++;
						num2Dmaps++;
					}
				}

				// Bind unassigned samplers to dummy/already bound textures so we don't get an invalid texture access error
				if (numCubemaps < Shader::maxPointLights)
				{
					int placeholderTexture;
					if (numCubemaps > 0) // We can just assign the rest of the cubemaps to an already bound cubemap and no need to use a texture slot on a dummy texture
					{
						placeholderTexture = depthCubemapSamplers[0];
					}
					else
					{
						glActiveTexture(GL_TEXTURE0 + textureUnit);
						glBindTexture(GL_TEXTURE_CUBE_MAP, resources.textures[resources.depth1x1Cubemap].id);
						placeholderTexture = textureUnit;
						textureUnit++;
					}

					for (int i = numCubemaps; i < Shader::maxPointLights; i++)
					{
						depthCubemapSamplers[i] = placeholderTexture;
					}
				}
				if (num2Dmaps < Shader::maxSpotLights + Shader::maxDirLights)
				{
					int placeholderTexture;
					if (num2Dmaps > 0)
					{
						// set it to either the first spot light texture (if there's a spot light depth map) or the first dir light texture
						placeholderTexture = depthMapSamplers[0] >= 0 ? depthMapSamplers[0] : depthMapSamplers[Shader::maxSpotLights];
					}
					else
					{
						glActiveTexture(GL_TEXTURE0 + textureUnit);
						glBindTexture(GL_TEXTURE_2D, resources.textures[resources.max1x1RedIndex].id);
						placeholderTexture = textureUnit;
						textureUnit++;
					}

					for (int i = 0; i < Shader::maxSpotLights + Shader::maxDirLights; i++)
					{
						if (depthMapSamplers[i] < 0)
						{
							depthMapSamplers[i] = placeholderTexture;
						}
					}
				}

				shader.SetIntArray("depthCubemaps", depthCubemapSamplers, Shader::maxPointLights);
				shader.SetIntArray("depthMaps", depthMapSamplers, Shader::maxSpotLights + Shader::maxDirLights);
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
			glBindVertexArray(submesh.VAO);
			if (submesh.hasIndexBuffer)
			{
				glDrawElements(GL_TRIANGLES, submesh.countVerticesOrIndices, GL_UNSIGNED_INT, nullptr);
			}
			else
			{
				glDrawArrays(GL_TRIANGLES, 0, submesh.countVerticesOrIndices);
			}
		}
	}

	//RenderBoundingBox(sceneBoundingBox, projection * view);
}

void Scene::RenderShadowMaps()
{
	for (int lightIdx = 0; lightIdx < lights.size(); lightIdx++)
	{
		Light& light = lights[lightIdx];
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBOs[lightIdx]);
		glViewport(0, 0, shadowMapWidth, shadowMapHeight);
		glClear(GL_DEPTH_BUFFER_BIT);

		glm::mat4& lightToWorld = globalTransforms[light.entityIdx];
		glm::vec3 forward = glm::normalize(lightToWorld[2]);
		glm::vec3 lightPositionWS = lightToWorld[3];
		assert(forward != glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 worldToLight = glm::lookAt(lightPositionWS, lightPositionWS + forward, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat4 projection;
		if (light.type != Light::Directional)
		{
			float fov = light.type == Light::Point ? 90.0f : light.depthmapFOV;
			projection = glm::perspective(glm::radians(fov), (float)shadowMapWidth / (float)shadowMapHeight, light.depthmapNearPlane, light.depthmapFarPlane);
		}
		else
		{
			// TODO: heuristic, fix later
			auto sceneBBVertices = sceneBoundingBox.GetVertices();
			float distFromCenter = 10.0f;
			float maxXDist = 0.0f;
			float maxYDist = 0.0f;
			do
			{
				lightPositionWS = sceneBoundingBox.GetCenter() - distFromCenter * forward;
				worldToLight = glm::lookAt(lightPositionWS, lightPositionWS + forward, glm::vec3(0.0f, 1.0f, 0.0f));
				light.depthmapFarPlane = 0.0f;
				light.depthmapNearPlane = FLT_MAX;
				maxXDist = 0.0f;
				maxYDist = 0.0f;
				for (const glm::vec3& vertex : sceneBBVertices)
				{
					glm::vec3 vertexLightSpace = worldToLight * glm::vec4(vertex, 1.0f);
					float vertexDepth = -vertexLightSpace.z; // camera looks into -z so convert to positive value to use as far plane
					if (vertexDepth > light.depthmapFarPlane)
					{
						light.depthmapFarPlane = vertexDepth;
					}
					if (vertexDepth < light.depthmapNearPlane)
					{
						light.depthmapNearPlane = vertexDepth;
					}

					float vertexXDist = std::abs(vertexLightSpace.x);
					if (vertexXDist > maxXDist)
					{
						maxXDist = vertexXDist;
					}

					float vertexYDist = std::abs(vertexLightSpace.y);
					if (vertexYDist > maxYDist)
					{
						maxYDist = vertexYDist;
					}
				}
				distFromCenter += 10.0f;
			} while (light.depthmapNearPlane <= 0.0f);

			
			// Make sure the frustum includes everything within the bounding box
			const float epsilon = 0.1f;
			float frustumWidth = maxXDist + epsilon;
			float frustumHeight = maxYDist + epsilon;
			light.depthmapFarPlane += epsilon;
			//light.shadowMappingBias = (light.depthmapFarPlane - light.depthmapNearPlane) * 0.01f;
			projection = glm::ortho(-frustumWidth, frustumWidth, -frustumHeight, frustumHeight, light.depthmapNearPlane, light.depthmapFarPlane);
		}

		light.lightProjection = projection * worldToLight;


		for (int entityIdx = 0; entityIdx < entities.size(); entityIdx++)
		{
			Entity& entity = entities[entityIdx];
			glm::mat4& entityGlobalTransform = globalTransforms[entityIdx];
			if (entity.meshIdx >= 0)
			{
				Mesh& mesh = resources.meshes[entity.meshIdx];
				for (Submesh& submesh : mesh.submeshes)
				{
					Shader& depthShader = resources.GetOrCreateDepthShader(submesh.flags, light.type == Light::Point);
					depthShader.use();
					if (light.type == Light::Point)
					{
						std::array<glm::mat4, 6> lightProjectionMatrices;
						// TODO: investigate up direction
						lightProjectionMatrices[0] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
						lightProjectionMatrices[1] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
						lightProjectionMatrices[2] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
						lightProjectionMatrices[3] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
						lightProjectionMatrices[4] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
						lightProjectionMatrices[5] = projection * glm::lookAt(lightPositionWS, lightPositionWS + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
						depthShader.SetMat4("lightProjectionMatrices[0]", glm::value_ptr(lightProjectionMatrices[0]));
						depthShader.SetMat4("lightProjectionMatrices[1]", glm::value_ptr(lightProjectionMatrices[1]));
						depthShader.SetMat4("lightProjectionMatrices[2]", glm::value_ptr(lightProjectionMatrices[2]));
						depthShader.SetMat4("lightProjectionMatrices[3]", glm::value_ptr(lightProjectionMatrices[3]));
						depthShader.SetMat4("lightProjectionMatrices[4]", glm::value_ptr(lightProjectionMatrices[4]));
						depthShader.SetMat4("lightProjectionMatrices[5]", glm::value_ptr(lightProjectionMatrices[5]));
						depthShader.SetMat4("transform", glm::value_ptr(entityGlobalTransform));
					}
					else
					{
						glm::mat4 worldLightProjection = light.lightProjection * entityGlobalTransform;
						depthShader.SetMat4("worldLightProjection", glm::value_ptr(worldLightProjection));
					}
					if (entity.skeletonIdx >= 0)
					{
						auto skinningMatrices = ComputeSkinningMatrices(skeletons[entity.skeletonIdx], entities);
						depthShader.SetMat4("skinningMatrices", glm::value_ptr(skinningMatrices.front()), (int)skinningMatrices.size());
					}
					bool hasMorphTargets = HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION);
					if (hasMorphTargets)
					{
						depthShader.SetFloat("morph1Weight", entity.morphTargetWeights[0]);
						depthShader.SetFloat("morph2Weight", entity.morphTargetWeights[1]);
					}
					glBindVertexArray(submesh.VAO);
					if (submesh.hasIndexBuffer)
					{
						glDrawElements(GL_TRIANGLES, submesh.countVerticesOrIndices, GL_UNSIGNED_INT, nullptr);
					}
					else
					{
						glDrawArrays(GL_TRIANGLES, 0, submesh.countVerticesOrIndices);
					}
				}
			}
		}
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
	UpdateGlobalTransforms();
	ComputeSceneBoundingBox();
	if (firstFrame)
	{
		firstFrame = false;
		ConfigureCamera(sceneBoundingBox);
	}
	Render(input.windowWidth, input.windowHeight);
	const glm::mat4 proj = currentCamera->GetProjectionMatrix();
	const glm::mat4 view = currentCamera->GetViewMatrix();
	const glm::mat4 projView = proj * view;
	RenderSkybox(view, proj);
	glDisable(GL_DEPTH_TEST);
	RenderSelectedEntityVisuals(projView);
	glEnable(GL_DEPTH_TEST);
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

	ImGui::Begin("Lighting");
	ImGui::SliderFloat("Exposure", &exposure, 0.0f, 10.0f);
	static std::vector<std::string> backgrounds = {
		"Environment map",
		"Irradiance map",
		"Prefilter map"
	};
	const int num_models = (int)backgrounds.size();
	auto& model_combo_preview_value = backgrounds[selectedBackgroundIdx];
	if (ImGui::BeginCombo("Background", model_combo_preview_value.c_str()))
	{
		for (int n = 0; n < num_models; n++)
		{
			const bool is_selected = (selectedBackgroundIdx == n);
			if (ImGui::Selectable(backgrounds[n].c_str(), is_selected))
			{
				selectedBackgroundIdx = n;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (selectedBackgroundIdx == 2) // prefilter map
	{
		ImGui::SliderFloat("Roughness", &prefilterMapRoughness, 0.0f, 1.0f);
	}
	ImGui::End();
	
	if (selectedEntityIdx >= 0)
	{
		Entity& selectedEntity = entities[selectedEntityIdx];
		ImGui::Begin("Components");

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// TODO: make speed based on scene size/adjustable by user
			ImGui::DragFloat3("Translation", &selectedEntity.transform.translation.x, 0.01f);
			glm::vec3 euler = glm::degrees(glm::eulerAngles(selectedEntity.transform.rotation));
			ImGui::DragFloat3("Rotation", &euler.x, 0.1f);
			selectedEntity.transform.rotation = glm::quat(glm::radians(euler));
			//ImGui::DragFloat4("Rotation(quat)", &selectedEntity.transform.rotation.x); // TODO: make this reasonable
			ImGui::DragFloat3("Scale", &selectedEntity.transform.scale.x, 0.1f);
		}

		if (selectedEntity.lightIdx >= 0)
		{
			Light& light = lights[selectedEntity.lightIdx];
			if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
			{
				constexpr int numLightTypes = 3;
				const char* lightTypeStrings[numLightTypes] = { "Point", "Spot", "Directional" };
				Light::Type oldType = light.type;
				ImGui::Combo("Type", (int*)&light.type, lightTypeStrings, numLightTypes);
				if (oldType != light.type)
				{
					// Shadow map only needs to be regenerated when we're switching from cube map to 2d map or vice versa
					// Old texture must be deleted and regenerated before binding it to a new texture type.
					if (oldType == Light::Point || light.type == Light::Point)
					{
						glDeleteTextures(1, &depthMaps[selectedEntity.lightIdx]);
						glGenTextures(1, &depthMaps[selectedEntity.lightIdx]);
						GenerateShadowMap(selectedEntity.lightIdx);
					}
				}
				ImGui::ColorPicker3("Color", &light.color.x);
				ImGui::InputFloat("Intensity", &light.intensity, 0.1f);
				light.intensity = std::max(light.intensity, 0.0f);
				if (light.type == Light::Point || light.type == Light::Spot)
				{
					ImGui::InputFloat("Range", &light.range, 0.1f);
					light.range = std::max(light.range, 0.0f);
				}
				if (light.type == Light::Spot)
				{
					ImGui::DragFloat("Inner cone angle", &light.innerAngleCutoffDegrees, 0.1f, 0.0f, light.outerAngleCutoffDegrees - 0.1f);
					ImGui::DragFloat("Outer cone angle", &light.outerAngleCutoffDegrees, 0.1f, light.innerAngleCutoffDegrees + 0.1f, 90.0f);
					ImGui::InputFloat("Depth Map FOV", &light.depthmapFOV, 0.1f);
				}

				//ImGui::InputFloat("Depth Map Near", &light.depthmapNearPlane, 0.000001f, );
				if (light.type == Light::Spot || light.type == Light::Point)
				{
					ImGui::InputFloat("Depth Map Far", &light.depthmapFarPlane, 0.01f);
				}
				ImGui::InputFloat("Shadow Mapping Bias", &light.shadowMappingBias, 0.0001f);

				if (light.type == Light::Point)
				{
					ImGui::DragInt("Shadow map render face", &light.debugShadowMapRenderFace, 0.2f, 0, 5);
				}
			}
		}

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

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
	{
		std::cout << entities[selectedEntityIdx].name;
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

void Scene::RenderFrustum(const glm::mat4& frustumViewProj, float near, float far, const glm::mat4& viewProj, bool perspective)
{
	// TODO: render shadow camera frustum. Remember: division by w inverted by division by Z (near plane depth for near plane vertices, far plane depth for far plane vertices).
	glm::mat4 clipToWorld = glm::inverse(frustumViewProj);
	glm::vec3 ndcCubeVertices[8] = {
		{-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f},
		{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},  {1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}
	};
	glm::vec3 frustumVertices[8];
	for (int i = 0; i < 8; i++)
	{
		frustumVertices[i] = ndcCubeVertices[i];
		if (perspective)
		{
			float clipW = frustumVertices[i].z < 0.0f ? near : far;
			frustumVertices[i] *= clipW;
			frustumVertices[i] = clipToWorld * glm::vec4(frustumVertices[i], clipW);
		}
		else
		{
			// clip space = ndc space for orthographic projection
			frustumVertices[i] = clipToWorld * glm::vec4(frustumVertices[i], 1.0f);
		}
	}
	glm::vec3 vbVertices[24];
	// Near plane lines
	vbVertices[0] = frustumVertices[0]; vbVertices[1] = frustumVertices[1];
	vbVertices[2] = frustumVertices[1]; vbVertices[3] = frustumVertices[2];
	vbVertices[4] = frustumVertices[2]; vbVertices[5] = frustumVertices[3];
	vbVertices[6] = frustumVertices[3]; vbVertices[7] = frustumVertices[0];

	// Far plane lines
	vbVertices[8] = frustumVertices[4]; vbVertices[9] = frustumVertices[5];
	vbVertices[10] = frustumVertices[5]; vbVertices[11] = frustumVertices[6];
	vbVertices[12] = frustumVertices[6]; vbVertices[13] = frustumVertices[7];
	vbVertices[14] = frustumVertices[7]; vbVertices[15] = frustumVertices[4];

	// Lines connecting far and near
	vbVertices[16] = frustumVertices[0]; vbVertices[17] = frustumVertices[4];
	vbVertices[18] = frustumVertices[1]; vbVertices[19] = frustumVertices[5];
	vbVertices[20] = frustumVertices[2]; vbVertices[21] = frustumVertices[6];
	vbVertices[22] = frustumVertices[3]; vbVertices[23] = frustumVertices[7];

	glBindVertexArray(frustumVAO);
	glBindBuffer(GL_ARRAY_BUFFER, frustumVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * 24, vbVertices);
	glm::mat4 mvp = viewProj;
	visualShader.use();
	visualShader.SetMat4("mvp", glm::value_ptr(mvp));
	visualShader.SetVec3("color", glm::vec3(0.0f, 1.0f, 0.0f));
	glDrawArrays(GL_LINES, 0, 24);
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
	if (entity.cameraIdx >= 0)
	{
		// We don't need to mess with yaw/pitch. Those are only used for changing camera via keyboard/mouse input. 
		// Entity cameras are not controllable by input; they only change when their entity's transform changes.
		Camera& camera = cameras[entity.cameraIdx];
		glm::vec4 x = glm::normalize(globalTransform[0]);
		glm::vec4 y = glm::normalize(globalTransform[1]);
		glm::vec4 z = glm::normalize(globalTransform[2]);
		camera.right = x;
		camera.up = y;
		camera.front = -z;
		camera.position = glm::vec3(globalTransform[3]);
	}
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

void Scene::RenderSelectedEntityVisuals(const glm::mat4& viewProj)
{
	if (selectedEntityIdx < 0) return;

	const Entity& entity = entities[selectedEntityIdx];
	const glm::mat4& globalTransform = globalTransforms[selectedEntityIdx];

	glBindFramebuffer(GL_FRAMEBUFFER, highlightFBO);
	glViewport(0, 0, fbW, fbH);
	HighlightEntityHierarchy(selectedEntityIdx, viewProj);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, fbW, fbH);
	
	glm::vec3 position = globalTransform[3];
	if (entity.lightIdx >= 0)
	{
		const Light& light = lights[entity.lightIdx];
		if (light.type == Light::Point)
		{
			glm::mat4 mvp = viewProj;
			mvp = glm::translate(mvp, position);
			mvp = glm::scale(mvp, glm::vec3(light.range));
			glBindVertexArray(circleVAO);
			visualShader.use();
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			visualShader.SetVec3("color", glm::vec3(1.0f, 0.0f, 0.0f));
			glDrawArrays(GL_LINE_LOOP, 0, numCircleVertices);
			glm::mat4 mvpXZCircle = glm::rotate(mvp, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			visualShader.SetMat4("mvp", glm::value_ptr(mvpXZCircle));
			glDrawArrays(GL_LINE_LOOP, 0, numCircleVertices);
			glm::mat4 mvpYZCircle = glm::rotate(mvp, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			visualShader.SetMat4("mvp", glm::value_ptr(mvpYZCircle));
			glDrawArrays(GL_LINE_LOOP, 0, numCircleVertices);

			// render frustum of current debug render face
			glm::mat4 projection = glm::perspective(glm::radians(90.0f), (float)shadowMapWidth / (float)shadowMapHeight, light.depthmapNearPlane, light.depthmapFarPlane);
			glm::mat4 view;
			if (light.debugShadowMapRenderFace == 0) // +x
			{
				view = glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			}
			else if (light.debugShadowMapRenderFace == 1) // -x
			{
				view = glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));

			}
			else if (light.debugShadowMapRenderFace == 2) // +y
			{
				view = glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

			}
			else if (light.debugShadowMapRenderFace == 3) // -y
			{
				view = glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

			}
			else if (light.debugShadowMapRenderFace == 4) // +z
			{

				view = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			}
			else // -z
			{
				view = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			}
			glm::mat4 lightProjection = projection * view;
			RenderFrustum(lightProjection, light.depthmapNearPlane, light.depthmapFarPlane, viewProj);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_CUBE_MAP, depthMaps[entity.lightIdx]);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE); // Treat as normal texture so we can visualize it
			
			GLuint debugRenderFaceTextureView;
			glGenTextures(1, &debugRenderFaceTextureView);
			glTextureView(debugRenderFaceTextureView, GL_TEXTURE_2D, depthMaps[entity.lightIdx], GL_DEPTH_COMPONENT24, 0, 1, light.debugShadowMapRenderFace, 1);
			glBindTexture(GL_TEXTURE_2D, debugRenderFaceTextureView);
			perspectiveDepthCubeMapShader.use();
			perspectiveDepthCubeMapShader.SetInt("depthMap", 0);
			perspectiveDepthCubeMapShader.SetFloat("nearPlane", light.depthmapNearPlane);
			perspectiveDepthCubeMapShader.SetFloat("farPlane", light.depthmapFarPlane);
			// Flip U and V if needed to align them with right-hand coordinate system (-z forward) https://www.khronos.org/opengl/wiki/Cubemap_Texture
			bool flipUV = light.debugShadowMapRenderFace == 0 || light.debugShadowMapRenderFace == 1 || light.debugShadowMapRenderFace == 4 || light.debugShadowMapRenderFace == 5;
			perspectiveDepthCubeMapShader.SetBool("flipUV", flipUV);
			glViewport(0, 0, shadowMapVisualizerDims, shadowMapVisualizerDims);
			glBindVertexArray(fullscreenQuadVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); // Treat as shadow texture
		}
		else if (light.type == Light::Spot)
		{
			glm::vec3 forward = glm::normalize(globalTransform[2]);
			glm::vec3 circleCenter = position + forward * light.range;
			glm::mat4 mvp = viewProj;
			mvp = glm::translate(mvp, circleCenter);
			glm::mat4 rotation = glm::mat4(glm::normalize(globalTransform[0]), glm::normalize(globalTransform[1]), glm::normalize(globalTransform[2]), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			mvp *= rotation;
			float radius = std::tan(glm::radians(light.outerAngleCutoffDegrees)) * light.range;
			mvp = glm::scale(mvp, glm::vec3(radius));
			glBindVertexArray(circleVAO);
			visualShader.use();
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			visualShader.SetVec3("color", glm::vec3(1.0f, 0.0f, 0.0f));
			glDrawArrays(GL_LINE_LOOP, 0, numCircleVertices);

			float lineLength = std::sqrt(light.range * light.range + radius * radius);
			glm::vec3 right = glm::normalize(globalTransform[0]);
			glm::vec3 up = glm::normalize(globalTransform[1]);
			float angle = glm::radians(45.0f);
			glm::vec3 lineEndPoint = glm::vec3(0.0f, 0.0f, 1.0f);

			glm::vec3 desiredLineEndPoint = circleCenter + radius * std::cos(angle) * right + radius * std::sin(angle) * up;
			glm::vec3 desiredLineVector = glm::normalize(desiredLineEndPoint - position);
			rotation = glm::mat4(1.0f);
			rotation[2] = glm::vec4(desiredLineVector, 0.0f);
			mvp = viewProj;
			mvp = glm::translate(mvp, position);
			mvp *= rotation;
			mvp = glm::scale(mvp, glm::vec3(lineLength));
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			glBindVertexArray(lineVAO);
			glDrawArrays(GL_LINES, 0, 2);

			angle += glm::radians(90.0f);
			desiredLineEndPoint = circleCenter + radius * std::cos(angle) * right + radius * std::sin(angle) * up;
			desiredLineVector = glm::normalize(desiredLineEndPoint - position);
			rotation[2] = glm::vec4(desiredLineVector, 0.0f);
			mvp = viewProj;
			mvp = glm::translate(mvp, position);
			mvp *= rotation;
			mvp = glm::scale(mvp, glm::vec3(lineLength));
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			glDrawArrays(GL_LINES, 0, 2);

			angle += glm::radians(90.0f);
			desiredLineEndPoint = circleCenter + radius * std::cos(angle) * right + radius * std::sin(angle) * up;
			desiredLineVector = glm::normalize(desiredLineEndPoint - position);
			rotation[2] = glm::vec4(desiredLineVector, 0.0f);
			mvp = viewProj;
			mvp = glm::translate(mvp, position);
			mvp *= rotation;
			mvp = glm::scale(mvp, glm::vec3(lineLength));
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			glDrawArrays(GL_LINES, 0, 2);

			angle += glm::radians(90.0f);
			desiredLineEndPoint = circleCenter + radius * std::cos(angle) * right + radius * std::sin(angle) * up;
			desiredLineVector = glm::normalize(desiredLineEndPoint - position);
			rotation[2] = glm::vec4(desiredLineVector, 0.0f);
			mvp = viewProj;
			mvp = glm::translate(mvp, position);
			mvp *= rotation;
			mvp = glm::scale(mvp, glm::vec3(lineLength));
			visualShader.SetMat4("mvp", glm::value_ptr(mvp));
			glDrawArrays(GL_LINES, 0, 2);

			RenderFrustum(light.lightProjection, light.depthmapNearPlane, light.depthmapFarPlane, viewProj);

			glViewport(0, 0, shadowMapVisualizerDims, shadowMapVisualizerDims);
			glBindVertexArray(fullscreenQuadVAO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMaps[entity.lightIdx]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE); // Treat as normal texture 
			perspectiveDepthMapShader.use();
			perspectiveDepthMapShader.SetInt("depthMap", 0);
			perspectiveDepthMapShader.SetFloat("nearPlane", light.depthmapNearPlane);
			perspectiveDepthMapShader.SetFloat("farPlane", light.depthmapFarPlane);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); // Treat as shadow texture
		}
		else
		{
			assert(light.type == Light::Directional);
			RenderFrustum(light.lightProjection, light.depthmapNearPlane, light.depthmapFarPlane, viewProj, false);

			glViewport(0, 0, shadowMapVisualizerDims, shadowMapVisualizerDims);
			glBindVertexArray(fullscreenQuadVAO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMaps[entity.lightIdx]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE); // Treat as normal texture 
			orthographicDepthMapShader.use();
			orthographicDepthMapShader.SetInt("depthMap", 0);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); // Treat as shadow texture
		}
	}

	if (entity.cameraIdx >= 0)
	{
		const Camera& camera = cameras[entity.cameraIdx];
		glm::mat4 cameraViewProj = camera.GetProjectionMatrix() * camera.GetViewMatrix();
		RenderFrustum(cameraViewProj, camera.near, camera.far, viewProj);
	}
}

void Scene::ConfigureCamera(const BBox& bbox)
{
	glm::vec3 dims = bbox.maxXYZ - bbox.minXYZ;
	glm::vec3 center = bbox.GetCenter();
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
	auto bboxPoints = bbox.GetVertices();
	while (true)
	{
		glm::mat4 mvp = controllableCamera.GetProjectionMatrix() * controllableCamera.GetViewMatrix();
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

void Scene::RenderSkybox(const glm::mat4& view, const glm::mat4& proj)
{
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glBindVertexArray(skyboxVAO);
	skyboxShader.use();
	const glm::mat4 rotView = glm::mat4(glm::mat3(view));
	skyboxShader.SetMat4("projection", glm::value_ptr(proj));
	skyboxShader.SetMat4("rotView", glm::value_ptr(rotView));
	glActiveTexture(GL_TEXTURE0);
	if (selectedBackgroundIdx == 0)
	{
		skyboxShader.SetFloat("levelOfDetail", 0.0f);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
	}
	else if (selectedBackgroundIdx == 1)
	{
		skyboxShader.SetFloat("levelOfDetail", 0.0f);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	}
	else
	{
		constexpr int maxLod = 4; // TODO: don't hardcode
		skyboxShader.SetFloat("levelOfDetail", prefilterMapRoughness * maxLod);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	}
	skyboxShader.SetInt("environmentMap", 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
}

void Scene::HighlightEntityHierarchy(int entityIdx, const glm::mat4& viewProj)
{
	const Entity& entity = entities[entityIdx];
	glm::mat4 mvp = viewProj * globalTransforms[entityIdx];
	if (entity.meshIdx >= 0)
	{
		const Mesh& mesh = resources.meshes[entity.meshIdx];
		for (const Submesh& submesh : mesh.submeshes)
		{
			Shader& highlightShader = resources.GetOrCreateHighlightShader(submesh.flags);
			highlightShader.use();
			highlightShader.SetMat4("transform", glm::value_ptr(mvp));

			if (entity.skeletonIdx >= 0)
			{
				auto skinningMatrices = ComputeSkinningMatrices(skeletons[entity.skeletonIdx], entities);
				highlightShader.SetMat4("skinningMatrices", glm::value_ptr(skinningMatrices.front()), (int)skinningMatrices.size());
			}
			bool hasMorphTargets = HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION);
			if (hasMorphTargets)
			{
				highlightShader.SetFloat("morph1Weight", entity.morphTargetWeights[0]);
				highlightShader.SetFloat("morph2Weight", entity.morphTargetWeights[1]);
			}
			glBindVertexArray(submesh.VAO);
			if (submesh.hasIndexBuffer)
			{
				glDrawElements(GL_TRIANGLES, submesh.countVerticesOrIndices, GL_UNSIGNED_INT, nullptr);
			}
			else
			{
				glDrawArrays(GL_TRIANGLES, 0, submesh.countVerticesOrIndices);
			}
		}
	}

	for (int i : entity.children)
	{
		HighlightEntityHierarchy(i, viewProj);
	}
}

void Scene::ComputeSceneBoundingBox()
{
	// TODO: reset bounding box, otherwise bounding box size never decreases even if scene bb actually does

	for (int i = 0; i < entities.size(); i++)
	{
		const Entity& entity = entities[i];
		if (entity.meshIdx < 0)
		{
			continue;
		}
		const glm::mat4& globalTransform = globalTransforms[i];
		const Mesh& entityMesh = resources.meshes[entity.meshIdx];
		auto bboxWorldVertices = entityMesh.boundingBox.GetVertices();

		for (glm::vec3& point : bboxWorldVertices)
		{
			// TODO: this is probably fine for static meshes but animated ones might have larger bounding boxes, so maybe fix that
			point = globalTransform * glm::vec4(point, 1.0f);
			sceneBoundingBox.minXYZ = glm::min(point, sceneBoundingBox.minXYZ);
			sceneBoundingBox.maxXYZ = glm::max(point, sceneBoundingBox.maxXYZ);
		}
	}

	glm::mat4 view = currentCamera->GetViewMatrix();
	// TODO: properly compute near plane
	/*if (sceneBoundingBox.Contains(currentCamera->position))
	{*/
		currentCamera->near = NEAR;
	//}
	//else
	//{
	//	float nearestDepth = FLT_MAX;
	//	
	//	const glm::mat4 view = currentCamera->GetViewMatrix();
	//	auto bboxWorldVertices = sceneBoundingBox.GetVertices();
	//	for (const glm::vec3& point : bboxWorldVertices)
	//	{
	//		// TODO: this is probably fine for static meshes but animated ones might have larger bounding boxes, so maybe fix that
	//		glm::vec3 viewSpaceVertex = view * glm::vec4(point, 1.0f);
	//		float vertexDepth = viewSpaceVertex.z;
	//		if (vertexDepth < 0.0f && -vertexDepth < nearestDepth)
	//		{
	//			nearestDepth = -vertexDepth;
	//		}
	//	}
	//	currentCamera->near = nearestDepth;
	//}
}

void Scene::GenerateShadowMap(int lightIdx)
{
	const Light& light = lights[lightIdx];
	if (light.type == Light::Point)
	{
		GLuint fbo = depthMapFBOs[lightIdx];
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		GLuint depthCubemapTexture = depthMaps[lightIdx];
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemapTexture);
		glTexStorage2D(GL_TEXTURE_CUBE_MAP, 6, GL_DEPTH_COMPONENT24, shadowMapWidth, shadowMapHeight);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemapTexture, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else
	{
		GLuint fbo = depthMapFBOs[lightIdx];
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		GLuint depthTexture = depthMaps[lightIdx];
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}
