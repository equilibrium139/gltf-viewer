#include <array>
#include <glad/glad.h>
#include <glm/vec3.hpp>

struct BBox
{
	glm::vec3 minXYZ, maxXYZ;
	std::array<glm::vec3, 8> GetPoints()
	{
		glm::vec3 dims = maxXYZ - minXYZ;
		return {
			minXYZ, minXYZ + glm::vec3(dims.x, 0.0f, 0.0f), minXYZ + glm::vec3(0.0f, 0.0f, dims.z), minXYZ + glm::vec3(dims.x, 0.0f, dims.z),
			maxXYZ, maxXYZ - glm::vec3(dims.x, 0.0f, 0.0f), maxXYZ - glm::vec3(0.0f, 0.0f, dims.z), maxXYZ - glm::vec3(dims.x, 0.0f, dims.z)
		};
	}
};