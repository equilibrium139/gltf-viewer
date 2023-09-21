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
	std::vector<Texture> textures;
	std::vector<PBRMaterial> materials;
	int white1x1RGBAIndex;
	int max1x1RedIndex;

	Shader& GetOrCreateShader(VertexAttribute attributes, bool flatShading);
};