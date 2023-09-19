#pragma once

#include <cmath>
#include <glm/glm.hpp>

// Padding added to match std140 layout

struct PointLight {
	glm::vec3 color;
	float range;
	glm::vec3 position;
	float intensity;

	PointLight(glm::vec3 color, glm::vec3 position, float range, float intensity)
		:color(color), range(range), position(position), intensity(intensity)  {}
};

struct DirectionalLight {
	glm::vec3 color;
	float intensity;
	glm::vec3 direction;
	float pad0;

	DirectionalLight(glm::vec3 color, glm::vec3 direction, float intensity)
		:color(color), direction(glm::normalize(direction)), intensity(intensity) {}
};

struct SpotLight {
	glm::vec3 color;
	float range;
	glm::vec3 position;
	float innerAngleCutoffDegrees;
	glm::vec3 direction;
	float outerAngleCutoffDegrees;
	float intensity;
	float pad0, pad1, pad2; // to account for padding between spotlight array and dirlight array

	SpotLight(glm::vec3 color, glm::vec3 position, glm::vec3 direction, float range, float innerAngleCutoffDegrees, float outerAngleCutoffDegrees, float intensity)
		:color(color), range(range), position(position), innerAngleCutoffDegrees(innerAngleCutoffDegrees), direction(glm::normalize(direction)), outerAngleCutoffDegrees(outerAngleCutoffDegrees), intensity(intensity) {}
};