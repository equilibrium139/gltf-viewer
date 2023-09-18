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
	Scene(const tinygltf::Scene& scene, const tinygltf::Model& model, int windowWidth, int windowHeight, GLuint fbo,
		GLuint fullscreenQuadVAO,
		GLuint colorTexture,
		GLuint highlightTexture,
		GLuint depthStencilRBO);
	void UpdateAndRender(const Input& input);
	float time = 0.0f; // TODO: remove
	float exposure = 1.0f;
private:
	void Render(int windowWidth, int windowHeight);
	void RenderEntity(const Entity& entity, const glm::mat4& parentTransform, const glm::mat4& view, const glm::mat4& projection, bool parentHighlighted = false);
	void RenderUI();
	void RenderHierarchyUI(const Entity& entity);
	void RenderBoundingBox(const BBox& bbox, const glm::mat4& mvp);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<Skeleton> skeletons;
	std::vector<Camera> cameras;
	std::vector<std::uint8_t> animationEnabled; // avoiding vector<bool> to allow imgui to have bool references to elements 
	Camera controllableCamera;
	Camera* currentCamera = &controllableCamera;
	GLTFResources resources;
	std::string selectedEntityName;
	GLuint boundingBoxVAO;
	Shader boundingBoxShader = Shader("Shaders/bbox.vert", "Shaders/bbox.frag");
	BBox sceneBoundingBox = {
		.minXYZ = glm::vec3(FLT_MAX),
		.maxXYZ = glm::vec3(-FLT_MAX),
	};
	GLuint fbo;
	GLuint fullscreenQuadVAO;
	GLuint colorTexture;
	GLuint highlightTexture;
	GLuint depthStencilRBO;
	int texW, texH;
	bool firstFrame = true;
};