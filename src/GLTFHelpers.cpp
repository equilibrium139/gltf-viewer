#include "GLTFHelpers.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

std::vector<std::uint8_t> GetAccessorBytes(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	int accessorTypeSize = GetAccessorTypeSizeInBytes(accessor);
	int numBytes = GetAccessorTypeSizeInBytes(accessor) * accessor.count;

	std::vector<std::uint8_t> data(numBytes);

	// Could be missing for sparse accessors
	if (accessor.bufferView < 0)
	{
		std::memset(&data[0], 0, numBytes);
	}
	else
	{
		const auto& bv = model.bufferViews[accessor.bufferView];
		const auto& buffer = model.buffers[bv.buffer];

		const std::uint8_t* gltfBufferPtr = buffer.data.data() + accessor.byteOffset + bv.byteOffset;
		const int stride = accessor.ByteStride(bv);

		std::uint8_t* dataPtr = &data[0];

		if (stride == accessorTypeSize || stride == 0) // tightly packed, simple copy
		{
			std::memcpy(&data[0], gltfBufferPtr, numBytes);
		}
		else
		{
			for (int i = 0; i < accessor.count; i++)
			{
				std::memcpy(dataPtr, gltfBufferPtr, accessorTypeSize);
				gltfBufferPtr += stride;
				dataPtr += accessorTypeSize;
			}
		}

	}

	if (accessor.sparse.isSparse)
	{
		const auto& valuesBufferView = model.bufferViews[accessor.sparse.values.bufferView];
		const auto& valuesBuffer = model.buffers[valuesBufferView.buffer];
		std::span<std::uint8_t> sparseValues((std::uint8_t*)(valuesBuffer.data.data() + accessor.sparse.values.byteOffset + valuesBufferView.byteOffset), 
												accessor.sparse.count);

		const auto& indicesBufferView = model.bufferViews[accessor.sparse.indices.bufferView];
		const auto& indicesBuffer = model.buffers[indicesBufferView.buffer];
		std::vector<std::uint32_t> sparseIndices(accessor.sparse.count);

		if (accessor.sparse.indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
		{
			std::span<std::uint8_t> indices((std::uint8_t*)(indicesBuffer.data.data() + accessor.sparse.indices.byteOffset + indicesBufferView.byteOffset),
				accessor.sparse.count);
			std::transform(indices.begin(), indices.end(), sparseIndices.begin(),
				[](std::uint8_t index)
				{
					return (std::uint32_t)index;
				});
		}
		else if (accessor.sparse.indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			std::span<std::uint16_t> indices((std::uint16_t*)(indicesBuffer.data.data() + accessor.sparse.indices.byteOffset + indicesBufferView.byteOffset),
				accessor.sparse.count);
			std::transform(indices.begin(), indices.end(), sparseIndices.begin(),
				[](std::uint16_t index)
				{
					return (std::uint32_t)index;
				});
		}
		else
		{
			assert(accessor.sparse.indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);

			std::span<std::uint32_t> indices((std::uint32_t*)(indicesBuffer.data.data() + accessor.sparse.indices.byteOffset + indicesBufferView.byteOffset),
				accessor.sparse.count);
			std::copy(indices.begin(), indices.end(), sparseIndices.begin());
		}

		const std::uint8_t* sparseValuesPtr = &sparseValues[0];
		for (int i = 0; i < accessor.sparse.count; i++)
		{
			int dataByteIndex = sparseIndices[i];
			std::uint8_t* dataPtr = &data[dataByteIndex * accessorTypeSize];
			std::memcpy(dataPtr, sparseValuesPtr, accessorTypeSize);
			sparseValuesPtr += accessorTypeSize;
		}

		return data;
	}

	return data;
}