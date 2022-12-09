#pragma once

#include "Mesh.h"
#include <string>
#include "Transform.h"
#include <vector>

struct Entity
{
	std::string name;
	Transform transform;
	std::vector<int> children;
	int parent;
	const Mesh* mesh = nullptr;
	std::vector<float> morphTargetWeights;
};