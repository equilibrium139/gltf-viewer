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

struct GLTFResources
{
	GLTFResources(const tinygltf::Model& model);
	std::vector<Mesh> meshes;
	std::unordered_map<VertexAttribute, Shader> shaders;
	std::vector<Texture> textures;
	std::vector<PBRMaterial> materials;
	int white1x1RGBAIndex;
	int max1x1RedIndex;

	Shader& GetMeshShader(const Mesh& mesh);
};