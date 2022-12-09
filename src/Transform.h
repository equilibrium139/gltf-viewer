#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <tiny_gltf/tiny_gltf.h>

struct Transform
{
	glm::vec3 translation;
	glm::vec3 scale;
	glm::quat rotation;

	glm::mat4 GetMatrix() const
	{
		glm::mat4 mat(1.0f);
		mat = glm::translate(mat, translation);
		mat = mat * glm::mat4(rotation);
		mat = glm::scale(mat, scale);
		return mat;
	}
};

Transform GetNodeTransform(const tinygltf::Node& node);