#pragma once

#include "Animation.h"
#include "Camera.h"
#include "Entity.h"
#include "GLTFResources.h"
#include "Input.h"
#include "Light.h"
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
		GLuint depthStencilRBO,
		GLuint lightsUBO);
	void UpdateAndRender(const Input& input);
	float time = 0.0f; // TODO: remove
	float exposure = 1.0f;
private:
	void Render(int windowWidth, int windowHeight);
	// Assumes global transforms are up to date
	void RenderShadowMaps(const glm::mat4& view);
	void RenderUI();
	void RenderHierarchyUI(int entityIdx);
	void RenderBoundingBox(const BBox& bbox, const glm::mat4& mvp);
	void UpdateGlobalTransforms();
	void UpdateGlobalTransforms(int entityIdx, const glm::mat4& parentTransform);
	bool IsParent(int entityChild, int entityParent);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<glm::mat4> globalTransforms;
	std::vector<Skeleton> skeletons;
	std::vector<Camera> cameras;
	std::vector<Light> lights;
	std::vector<GLuint> depthMapFBOs;
	std::vector<GLuint> depthMaps;
	std::vector<std::uint8_t> animationEnabled; // avoiding vector<bool> to allow imgui to have bool references to elements 
	Camera controllableCamera;
	Camera* currentCamera = &controllableCamera;
	GLTFResources resources;
	int selectedEntityIdx = -1;
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
	GLuint lightsUBO;
	int texW, texH;
	bool firstFrame = true;
	// TODO: make shadow map size tweakable? And in general allow for shadow options like toggling shadows
	static constexpr int shadowMapWidth = 2048;
	static constexpr int shadowMapHeight = 2048;
};