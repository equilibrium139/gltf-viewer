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
	const Mesh* mesh = nullptr;
	const Skeleton* skeleton = nullptr;
	std::vector<float> morphTargetWeights;
};