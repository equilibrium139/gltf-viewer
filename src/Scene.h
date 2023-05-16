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
	void RenderHierarchyUI(const Entity& entity);
	void RenderBoundingBox(const BBox& bbox, const glm::mat4& mvp);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<Skeleton> skeletons;
	GLTFResources resources;
	std::string selectedEntityName;
	int currentAnimationIdx = 0;
	GLuint boundingBoxVAO;
	Shader boundingBoxShader = Shader("Shaders/bbox.vert", "Shaders/bbox.frag");
	BBox sceneBoundingBox;
};