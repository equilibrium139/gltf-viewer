#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "Skeleton.h"
#include "tiny_gltf/tiny_gltf.h"
#include "VertexAttribute.h"
#include <vector>
#include <unordered_map>

struct GLTFResources
{
	GLTFResources(const tinygltf::Model& model);
	std::vector<Mesh> meshes;
	std::unordered_map<VertexAttribute, Shader> shaders;

	Shader& GetMeshShader(const Mesh& mesh);
};