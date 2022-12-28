#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <tiny_gltf/tiny_gltf.h>
#include "VertexAttribute.h"

struct Submesh
{
	int start, countVerticesOrIndices;
	// TODO: add material
};

struct Mesh
{
	Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);
	std::vector<std::uint32_t> submeshCountVerticesOrIndices;
	GLuint VAO;
	VertexAttribute flags = VertexAttribute::POSITION;
	bool hasIndexBuffer;
};