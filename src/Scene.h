#pragma once

#include "Animation.h"
#include "Camera.h"
#include "Entity.h"
#include "GLTFResources.h"
#include "Input.h"
#include "Shader.h"
#include "Skeleton.h"
#include "tiny_gltf/tiny_gltf.h"
#include <vector>

class Scene
{
public:
	Scene(const tinygltf::Scene& scene, const tinygltf::Model& model);
	void UpdateAndRender(const Input& input);
	Camera camera;
	float time = 0.0f; // TODO: remove
private:
	void Render(float aspectRatio);
	void RenderEntity(const Entity& entity, const glm::mat4& parentTransform, const glm::mat4& view, const glm::mat4& projection);
	void RenderUI();
	void RenderSceneHierarchy(const Entity& entity);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<Skeleton> skeletons;
	GLTFResources resources;
};