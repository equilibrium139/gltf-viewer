#pragma once

#include <cstdint>
#include <glad/glad.h>
#include "PBRMaterial.h"
#include <tiny_gltf/tiny_gltf.h>
#include "VertexAttribute.h"

struct Submesh
{
	int start, countVerticesOrIndices;
	int materialIndex;
};

struct Mesh
{
	Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);
	std::vector<Submesh> submeshes;
	GLuint VAO;
	GLuint VBO;
	GLuint IBO;
	VertexAttribute flags = VertexAttribute::POSITION;
	bool hasIndexBuffer;
	bool flatShading = false;
};