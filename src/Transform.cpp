#include "Transform.h"

#include <glm/gtx/matrix_decompose.hpp>

Transform GetNodeTransform(const tinygltf::Node& node)
{
	Transform transform{
		.translation = {0.0f, 0.0f, 0.0f},
		.scale = {1.0f, 1.0f, 1.0f},
		.rotation = glm::identity<glm::quat>()
	};

	if (node.matrix.size() == 16)
	{
		glm::mat4 mat(1.0f);

		mat[0][0] = node.matrix[0];
		mat[0][1] = node.matrix[1];
		mat[0][2] = node.matrix[2];
		mat[0][3] = node.matrix[3];
		mat[1][0] = node.matrix[4];
		mat[1][1] = node.matrix[5];
		mat[1][2] = node.matrix[6];
		mat[1][3] = node.matrix[7];
		mat[2][0] = node.matrix[8];
		mat[2][1] = node.matrix[9];
		mat[2][2] = node.matrix[10];
		mat[2][3] = node.matrix[11];
		mat[3][0] = node.matrix[12];
		mat[3][1] = node.matrix[13];
		mat[3][2] = node.matrix[14];
		mat[3][3] = node.matrix[15];

		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(mat, transform.scale, transform.rotation, transform.translation, skew, perspective);
	}
	else
	{
		if (node.translation.size() == 3)
		{
			transform.translation.x = node.translation[0];
			transform.translation.y = node.translation[1];
			transform.translation.z = node.translation[2];
		}
		if (node.rotation.size() == 4)
		{
			transform.rotation.w = node.rotation[3];
			transform.rotation.x = node.rotation[0];
			transform.rotation.y = node.rotation[1];
			transform.rotation.z = node.rotation[2];
		}
		if (node.scale.size() == 3)
		{
			transform.scale.x = node.scale[0];
			transform.scale.y = node.scale[1];
			transform.scale.z = node.scale[2];
		}
	}

	return transform;
}