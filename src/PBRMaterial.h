#pragma once

#include <glm/vec4.hpp>
#include <string>
#include "Texture.h"

struct PBRMaterial
{
	std::string name;
	glm::vec4 baseColorFactor;
	int baseColorTextureIdx;
	float metallicFactor;
	float roughnessFactor;
	int metallicRoughnessTextureIdx;
	int normalTextureIdx;

	// TODO: add support for occlusion and emissive textures
};