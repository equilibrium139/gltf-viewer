#pragma once

#include "Mesh.h"
#include "PBRMaterial.h"
#include "Shader.h"
#include "Skeleton.h"
#include "Texture.h"
#include "tiny_gltf/tiny_gltf.h"
#include "VertexAttribute.h"
#include <vector>
#include <unordered_map>
#include <utility>

// TODO: just make this part of Scene?
struct GLTFResources
{
	GLTFResources(const tinygltf::Model& model);
	std::vector<Mesh> meshes;
	// TODO: make shader depend on material as well
	using ShaderKey = std::pair<VertexAttribute, bool>; // bool = flatShading
	std::vector<std::pair<ShaderKey, Shader>> shaders;
	using DepthShaderKey = std::pair<VertexAttribute, bool>; // bool = depth cubemap (for point lights)
	std::vector<std::pair<DepthShaderKey, Shader>> depthShaders;
	std::vector<Texture> textures;
	std::vector<PBRMaterial> materials;
	// TODO: probably should make these global, these aren't specific to each scene
	int white1x1RGBAIndex;
	int max1x1RedIndex;
	
	// This texture is used as a dummy to ensure that uninitialized samplerCube/sampler2D variables are bound to cubemaps/2D textures respectively. Otherwise we get invalid texture access error
	// because for example an uninitialized samplerCube might be bound to a 2D texture, even if that samplerCube is not used (because of a conditional for example).
	int depth1x1Cubemap;

	Shader& GetOrCreateShader(VertexAttribute attributes, bool flatShading);
	Shader& GetOrCreateDepthShader(VertexAttribute attributes, bool depthCubemap);
};