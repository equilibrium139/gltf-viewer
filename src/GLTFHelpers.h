#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <tiny_gltf/tiny_gltf.h>
#include <type_traits>
#include <utility>
#include <vector>

inline int GetAccessorTypeSizeInBytes(const tinygltf::Accessor& accessor)
{
	return tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
}

std::vector<std::uint8_t> GetAccessorBytes(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
