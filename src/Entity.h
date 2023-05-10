#pragma once

#include "Mesh.h"
#include "Skeleton.h"
#include <string>
#include "Transform.h"
#include <vector>

struct Entity
{
	std::string name;
	Transform transform;
	std::vector<int> children;
	int parent = -1;
	int meshIdx = -1;
	int skeletonIdx = -1;
	std::vector<float> morphTargetWeights;
};