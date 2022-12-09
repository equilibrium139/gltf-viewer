#include "Mesh.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include "GLTFHelpers.h"
#include <utility>
#include <vector>
#include <span>

static const std::unordered_map<std::string, VertexAttribute> vertexAttributeMapping =
{
	{"POSITION", VertexAttribute::POSITION},
	{"NORMAL", VertexAttribute::NORMAL},
};
static const std::vector<VertexAttribute> vertexAttributeOrdering =
{
	VertexAttribute::POSITION,
	VertexAttribute::NORMAL,
	VertexAttribute::MORPH_TARGET0_POSITION,
	VertexAttribute::MORPH_TARGET1_POSITION,
	VertexAttribute::MORPH_TARGET0_NORMAL,
	VertexAttribute::MORPH_TARGET1_NORMAL
};
static const std::unordered_map<VertexAttribute, int> attributeByteSizes =
{
	{VertexAttribute::POSITION, 12},
	{VertexAttribute::NORMAL, 12},
	{VertexAttribute::MORPH_TARGET0_POSITION, 12},
	{VertexAttribute::MORPH_TARGET1_POSITION, 12},
	{VertexAttribute::MORPH_TARGET0_NORMAL, 12},
	{VertexAttribute::MORPH_TARGET1_NORMAL, 12}
};

static int GetAttributeByteOffset(VertexAttribute attributes, VertexAttribute attribute)
{
	int offset = 0;

	for (VertexAttribute attr : vertexAttributeOrdering)
	{
		if (attribute == attr)
		{
			return offset;
		}
		
		offset += attributeByteSizes.find(attr)->second;
	}

	assert(false && "Attribute not found");
	return -1;
}

static VertexAttribute GetPrimitiveVertexAttributes(const tinygltf::Primitive& primitive)
{
	VertexAttribute attributes = (VertexAttribute)0;

	for (const auto& attribute : primitive.attributes)
	{
		const std::string& attributeName = attribute.first;
		auto iter = std::find_if(vertexAttributeMapping.begin(), vertexAttributeMapping.end(), [&attributeName](const auto& mapping) { return mapping.first == attributeName; });
		bool attributeSupported = iter != vertexAttributeMapping.end();
		if (attributeSupported)
		{
			attributes |= iter->second;
		}
	}

	int countMorphTargets = primitive.targets.size();
	if (countMorphTargets > 0)
	{
		assert(countMorphTargets == 2 && "Only 2 morph targets per primitive currently supported");
		attributes |= VertexAttribute::MORPH_TARGET0_POSITION;
		attributes |= VertexAttribute::MORPH_TARGET1_POSITION;
		if (HasFlag(attributes, VertexAttribute::NORMAL))
		{
			assert(primitive.targets[0].contains("NORMAL") && primitive.targets[1].contains("NORMAL"));
			attributes |= VertexAttribute::MORPH_TARGET0_NORMAL;
			attributes |= VertexAttribute::MORPH_TARGET1_NORMAL;
		}
	}
	
	return attributes;
}

static int GetVertexSizeBytes(VertexAttribute attributes)
{
	int size = 0;

	if (HasFlag(attributes, VertexAttribute::POSITION)) size += 12;
	if (HasFlag(attributes, VertexAttribute::NORMAL)) size += 12;
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_POSITION)) size += 12;
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_POSITION)) size += 12;
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_NORMAL)) size += 12;
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_NORMAL)) size += 12;

	return size;
}

static void FillInterleavedBufferWithAttribute(std::vector<std::uint8_t>& interleavedBuffer, const tinygltf::Accessor& attrAccessor, int vertexSizeBytes, int attrOffset, const tinygltf::Model& model)
{
	const tinygltf::BufferView& accessorBV = model.bufferViews[attrAccessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[accessorBV.buffer];

	const std::uint8_t* gltfBufferAttrPtr = buffer.data.data() + accessorBV.byteOffset + attrAccessor.byteOffset;
	const int accessorStride = attrAccessor.ByteStride(accessorBV);

	std::uint8_t* interleavedBufferAttrPtr = interleavedBuffer.data() + attrOffset;
	const int interleavedAttributeStride = vertexSizeBytes;

	int accessorTypeSizeBytes = tinygltf::GetComponentSizeInBytes(attrAccessor.componentType) * tinygltf::GetNumComponentsInType(attrAccessor.type);

	for (int i = 0; i < attrAccessor.count; i++)
	{
		std::memcpy(interleavedBufferAttrPtr, gltfBufferAttrPtr, accessorTypeSizeBytes);
		interleavedBufferAttrPtr += interleavedAttributeStride;
		gltfBufferAttrPtr += accessorStride;
	}
}

static std::vector<std::uint8_t> GetInterleavedVertexBuffer(const tinygltf::Primitive& primitive, VertexAttribute attributes, const tinygltf::Model& model)
{
	assert(HasFlag(attributes, VertexAttribute::POSITION) && "Assuming all primitives have position attribute.");

	int vertexSizeBytes = GetVertexSizeBytes(attributes);

	int positionAccessorIndex = primitive.attributes.find("POSITION")->second;
	const tinygltf::Accessor& positionsAccessor = model.accessors[positionAccessorIndex];
	const int numVertices = positionsAccessor.count;

	std::vector<std::uint8_t> buffer(vertexSizeBytes * numVertices);

	FillInterleavedBufferWithAttribute(buffer, positionsAccessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::POSITION), model);
	
	if (HasFlag(attributes, VertexAttribute::NORMAL))
	{
		int normalAccessorIndex = primitive.attributes.find("NORMAL")->second;
		const tinygltf::Accessor& normalsAccessor = model.accessors[normalAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, normalsAccessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::NORMAL), model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_POSITION))
	{
		int accessorIndex = primitive.targets[0].find("POSITION")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::MORPH_TARGET0_POSITION), model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_POSITION))
	{
		int accessorIndex = primitive.targets[1].find("POSITION")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::MORPH_TARGET1_POSITION), model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_NORMAL))
	{
		int accessorIndex = primitive.targets[0].find("NORMAL")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::MORPH_TARGET0_NORMAL), model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_NORMAL))
	{
		int accessorIndex = primitive.targets[1].find("NORMAL")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, GetAttributeByteOffset(attributes, VertexAttribute::MORPH_TARGET1_NORMAL), model);
	}

	return buffer;
}

static std::vector<std::uint32_t> GetIndexBuffer(const tinygltf::Primitive& primitive, const tinygltf::Model& model, int offset)
{
	const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
	assert(indicesAccessor.byteOffset == 0);
	std::vector<std::uint32_t> indexBuffer(indicesAccessor.count);
	int componentSizeBytes = tinygltf::GetComponentSizeInBytes(indicesAccessor.componentType);
	const tinygltf::BufferView& indicesBV = model.bufferViews[indicesAccessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[indicesBV.buffer];

	if (componentSizeBytes == 4)
	{
		std::span<std::uint32_t> indicesAccessorData((std::uint32_t*)(buffer.data.data() + indicesBV.byteOffset), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(), 
			[offset](std::uint32_t index) { return index + offset; });
	}
	else if (componentSizeBytes == 2)
	{
		std::span<std::uint16_t> indicesAccessorData((std::uint16_t*)(buffer.data.data() + indicesBV.byteOffset), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(),
			[offset](std::uint16_t index) { return std::uint32_t(index) + offset; });
	}
	else
	{
		assert(componentSizeBytes == 1 && "Invalid index buffer component size.");
		std::span<std::uint8_t> indicesAccessorData((std::uint8_t*)(buffer.data.data() + indicesBV.byteOffset), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(),
			[offset](std::uint8_t index) { return std::uint32_t(index) + offset; });
	}
	
	return indexBuffer;
}

// Create one interleaved vertex buffer that contains the vertices of all the mesh's primitives
Mesh::Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	assert(mesh.primitives.size() > 0);

	flags = GetPrimitiveVertexAttributes(mesh.primitives[0]);
	const int vertexSizeBytes = GetVertexSizeBytes(flags);
	hasIndexBuffer = mesh.primitives[0].indices >= 0;

	// Contains vertices of all primitives of this mesh
	std::vector<std::uint8_t> vertexBuffer;
	std::vector<std::uint32_t> indexBuffer;

	int primitiveIndicesOffset = 0;

	for (const tinygltf::Primitive& primitive : mesh.primitives)
	{
		assert(primitive.mode == GL_TRIANGLES);
		assert(primitive.indices >= 0 == hasIndexBuffer && "Mesh primitives must all have indices or all not have them.");
		VertexAttribute primitiveFlags = GetPrimitiveVertexAttributes(primitive);
		assert(flags == primitiveFlags && "All mesh primitives must have the same attributes");
		std::vector<std::uint8_t> primitiveVertexBuffer = GetInterleavedVertexBuffer(primitive, primitiveFlags, model);
		vertexBuffer.insert(vertexBuffer.end(), primitiveVertexBuffer.begin(), primitiveVertexBuffer.end());

		if (hasIndexBuffer)
		{
			std::vector<std::uint32_t> primitiveIndexBuffer = GetIndexBuffer(primitive, model, primitiveIndicesOffset);
			indexBuffer.insert(indexBuffer.end(), primitiveIndexBuffer.begin(), primitiveIndexBuffer.end());
			submeshCountVerticesOrIndices.push_back(primitiveIndexBuffer.size());
			int vertexBufferBytes = vertexBuffer.size();
			int countVertices = vertexBufferBytes / vertexSizeBytes;
			primitiveIndicesOffset += countVertices;
		}
		else
		{
			int primitiveVertexBufferBytes = primitiveVertexBuffer.size();
			int countPrimitiveVertices = primitiveVertexBufferBytes / vertexSizeBytes;
			submeshCountVerticesOrIndices.push_back(countPrimitiveVertices);
		}
	}

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size(), vertexBuffer.data(), GL_STATIC_DRAW);

	int offset = 0;
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
	offset += 12;

	if (HasFlag(flags, VertexAttribute::NORMAL))
	{
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += 12;
	}
	if (HasFlag(flags, VertexAttribute::MORPH_TARGET0_POSITION))
	{
		assert(HasFlag(flags, VertexAttribute::MORPH_TARGET1_POSITION));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += 12;
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += 12;
	}
	if (HasFlag(flags, VertexAttribute::MORPH_TARGET0_NORMAL))
	{
		assert(HasFlag(flags, VertexAttribute::MORPH_TARGET1_NORMAL));
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += 12;
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += 12;
	}
	
	if (hasIndexBuffer)
	{
		GLuint IBO;
		glGenBuffers(1, &IBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size() * sizeof(indexBuffer[0]), indexBuffer.data(), GL_STATIC_DRAW);
	}
}