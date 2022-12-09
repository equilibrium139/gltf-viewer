#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <tiny_gltf/tiny_gltf.h>
#include <type_traits>
#include <utility>

inline int GetTypeSizeInBytes(const tinygltf::Accessor& accessor)
{
	return tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
}

template<typename T>
inline std::span<T> GetAccessorDataView(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	int typeSizeBytes = GetTypeSizeInBytes(accessor);
	assert(sizeof(T) == typeSizeBytes);

	const auto& bufferView = model.bufferViews[accessor.bufferView];
	const auto& buffer = model.buffers[bufferView.buffer];
	assert(accessor.ByteStride(bufferView) == typeSizeBytes && "Can't return span if data is not tightly packed");

	return { (T*) (buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count };
}

