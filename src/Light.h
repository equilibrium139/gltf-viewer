#pragma once

#include <cmath>
#include <glm/glm.hpp>

// distinction between interface lights and GPU lights (interface lights may have easier to tweak data while GPU lights have
// exactly what the shader needs to compute lighting and nothing more (other than padding to exactly match GPU format)


// Interface light
struct Light {
	enum Type : int {
		Point, 
		Spot,
		Directional
	};

	Type type = Type::Point;
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float range = 10.0f;
	float innerAngleCutoffDegrees = 0.0f;
	float outerAngleCutoffDegrees = 45.0f;
	int entityIdx; // must be >= 0
};

// GPU lights (for use in shaders). Padding added to match std140 layout
struct PointLight {
	glm::vec3 color;
	float range;
	glm::vec3 positionVS;
	float intensity;

	PointLight(glm::vec3 color, glm::vec3 position, float range, float intensity)
		:color(color), range(range), positionVS(position), intensity(intensity)  {}
};

struct SpotLight {
	glm::vec3 color;
	float range;
	glm::vec3 positionVS;
	float lightAngleScale;
	glm::vec3 directionVS;
	float lightAngleOffset;
	float intensity;
	float pad0, pad1, pad2; // to account for padding between spotlight array and dirlight array

	SpotLight(glm::vec3 color, glm::vec3 position, glm::vec3 direction, float range, float innerAngleCutoffDegrees, float outerAngleCutoffDegrees, float intensity)
		:color(color), range(range), positionVS(position), directionVS(glm::normalize(direction)), intensity(intensity)
	{
		// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
		const float cosInner = std::cos(glm::radians(innerAngleCutoffDegrees));
		const float cosOuter = std::cos(glm::radians(outerAngleCutoffDegrees));
		lightAngleScale = 1.0f / std::max(0.0001f, cosInner - cosOuter);
		lightAngleOffset = -cosOuter * lightAngleScale;
	}
};

struct DirectionalLight {
	glm::vec3 color;
	float intensity;
	glm::vec3 directionVS;
	float pad0;

	DirectionalLight(glm::vec3 color, glm::vec3 direction, float intensity)
		:color(color), directionVS(glm::normalize(direction)), intensity(intensity) {}
};
