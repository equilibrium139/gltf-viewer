#pragma once

#include "Animation.h"
#include "Camera.h"
#include "Entity.h"
#include "GLTFResources.h"
#include "Shader.h"
#include "Skeleton.h"
#include "tiny_gltf/tiny_gltf.h"
#include <vector>

class Scene
{
public:
	Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, GLTFResources* resources);
	void Render(float aspectRatio);
	void Update(float dt);
	Camera camera;
	float time = 0.0f; // TODO: remove
private:
	void RenderEntity(const Entity& entity, const glm::mat4& parentTransform, const glm::mat4& view, const glm::mat4& projection);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<Skeleton> skeletons;
	GLTFResources* resources;
};