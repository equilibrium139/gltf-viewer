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
	void RenderFrustum(const glm::mat4& frustumViewProj, float near, float far, const glm::mat4& viewProj, bool perspective = true);
	void UpdateGlobalTransforms();
	void UpdateGlobalTransforms(int entityIdx, const glm::mat4& parentTransform);
	void ComputeSceneBoundingBox();
	void GenerateShadowMap(int lightIdx);
	bool IsParent(int entityChild, int entityParent);
	void RenderSelectedEntityVisuals(const glm::mat4& viewProj);
	std::vector<Animation> animations;
	std::vector<Entity> entities;
	std::vector<glm::mat4> globalTransforms;
	std::vector<Skeleton> skeletons;
	std::vector<Camera> cameras;
	std::vector<Light> lights;
	std::vector<GLuint> depthMapFBOs; // TODO: sync these to lights in a smarter way. Switching light type poses problems for how it's currently being done
	std::vector<GLuint> depthMaps;
	std::vector<std::uint8_t> animationEnabled; // avoiding vector<bool> to allow imgui to have bool references to elements 
	Camera controllableCamera;
	Camera* currentCamera = &controllableCamera;
	GLTFResources resources;
	int selectedEntityIdx = -1;
	GLuint boundingBoxVAO;
	Shader boundingBoxShader = Shader("Shaders/bbox.vert", "Shaders/bbox.frag"); // TODO: rename this to something general
	BBox sceneBoundingBox = {
		.minXYZ = glm::vec3(FLT_MAX),
		.maxXYZ = glm::vec3(-FLT_MAX),
	};
	GLuint fbo;
	GLuint fullscreenQuadVAO; // TODO: remove
	GLuint colorTexture;
	GLuint highlightTexture;
	GLuint depthStencilRBO;
	GLuint lightsUBO;
	GLuint circleVAO; // TODO: find a better place for visual vertex buffers
	const int numCircleVertices = 200;
	GLuint lineVAO;
	GLuint frustumVAO;
	GLuint frustumVBO;
	int texW, texH; // TODO: fix this nonsensical naming
	bool firstFrame = true;
	// TODO: make shadow map size tweakable? And in general allow for shadow options like toggling shadows
	static constexpr int shadowMapWidth = 2048;
	static constexpr int shadowMapHeight = 2048;
};