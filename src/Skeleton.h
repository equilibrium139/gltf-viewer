#pragma once

#include <glm/mat4x3.hpp>
#include <string>
#include <vector>

struct Joint
{
	glm::mat4x3 localToJoint; // AKA "inverse bind matrix". Apparently 4x3 actually means 3 rows 4 columns in glm so this is fine
	int entityIndex;
	int parent; // not to be confused with entity.parent. That refers to the scene's entity hierarachy, while this refers to the skeleton's joint hierarchy
};

struct Skeleton
{
	std::vector<Joint> joints;
};