#pragma once

#include "BBox.h"
#include <cstdint>
#include <glad/glad.h>
#include "PBRMaterial.h"
#include <tiny_gltf/tiny_gltf.h>
#include "VertexAttribute.h"

struct Submesh
{
	GLuint VAO;
	VertexAttribute flags = VertexAttribute::POSITION;
	int countVerticesOrIndices;
	int materialIndex;
	bool hasIndexBuffer;
	bool flatShading = false;
};

struct Mesh
{
	Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);
	std::vector<Submesh> submeshes;
	BBox boundingBox {
		.minXYZ = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX),
		.maxXYZ = glm::vec3(FLT_MIN, FLT_MIN, FLT_MIN)
	};
	bool HasMorphTargets();
};