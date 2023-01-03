#include "Mesh.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include "GLTFHelpers.h"
#include <utility>
#include <vector>
#include <span>

// Used for glTF attribute names
static const std::unordered_map<std::string, VertexAttribute> vertexAttributeMapping =
{
	{"POSITION", VertexAttribute::POSITION},
	{"TEXCOORD_0",  VertexAttribute::TEXCOORD},
	{"NORMAL", VertexAttribute::NORMAL},
	{"JOINTS_0", VertexAttribute::WEIGHTS},
	{"WEIGHTS_0", VertexAttribute::JOINTS}
};
static const std::vector<VertexAttribute> vertexAttributeOrdering =
{
	VertexAttribute::POSITION,
	VertexAttribute::TEXCOORD,
	VertexAttribute::NORMAL,
	VertexAttribute::WEIGHTS,
	VertexAttribute::JOINTS,
	VertexAttribute::MORPH_TARGET0_POSITION,
	VertexAttribute::MORPH_TARGET1_POSITION,
	VertexAttribute::MORPH_TARGET0_NORMAL,
	VertexAttribute::MORPH_TARGET1_NORMAL
};
static const std::unordered_map<VertexAttribute, int> attributeByteSizes =
{
	{VertexAttribute::POSITION, 12},
	{VertexAttribute::TEXCOORD, 8},
	{VertexAttribute::NORMAL, 12},
	{VertexAttribute::WEIGHTS, 16},
	{VertexAttribute::JOINTS, 4},
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
		
		if (HasFlag(attributes, attr))
		{
			offset += attributeByteSizes.find(attr)->second;
		}
	}

	assert(false && "Attribute not found");
	return -1;
}

static VertexAttribute GetPrimitiveVertexLayout(const tinygltf::Primitive& primitive)
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

	for (const auto& pair : attributeByteSizes)
	{
		if (HasFlag(attributes, pair.first))
		{
			size += pair.second;
		}
	}

	return size;
}

static void FillInterleavedBufferWithAttribute(std::vector<std::uint8_t>& interleavedBuffer, std::span<const std::uint8_t> attrData, int attrSizeBytes, int attrOffset, int vertexSizeBytes, int numVertices)
{
	const std::uint8_t* attrDataPtr = attrData.data();

	std::uint8_t* interleavedBufferAttrPtr = interleavedBuffer.data() + attrOffset;
	const int interleavedAttributeStride = vertexSizeBytes;

	for (int i = 0; i < numVertices; i++)
	{
		std::memcpy(interleavedBufferAttrPtr, attrDataPtr, attrSizeBytes);
		interleavedBufferAttrPtr += interleavedAttributeStride;
		attrDataPtr += attrSizeBytes;
	}
}

static void FillInterleavedBufferWithAttribute(std::vector<std::uint8_t>& interleavedBuffer, const tinygltf::Accessor& accessor, int vertexSizeBytes, VertexAttribute attribute, VertexAttribute attributes, const tinygltf::Model& model)
{
	// Positions, normals, and tangents are always float vec3 so we can always treat them the same, but the other types can have different component types
	// so they need to be converted to a single type
	switch (attribute)
	{
	case VertexAttribute::POSITION: case VertexAttribute::NORMAL: 
	case VertexAttribute::MORPH_TARGET0_POSITION: case VertexAttribute::MORPH_TARGET1_POSITION:
	case VertexAttribute::MORPH_TARGET0_NORMAL: case VertexAttribute::MORPH_TARGET1_NORMAL: // TODO: add tangents
		FillInterleavedBufferWithAttribute(interleavedBuffer, GetAccessorBytes(accessor, model), attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		break;
	case VertexAttribute::WEIGHTS: case VertexAttribute::TEXCOORD:
		assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && "Normalized unsigned byte and unsigned short not supported for now");
		FillInterleavedBufferWithAttribute(interleavedBuffer, GetAccessorBytes(accessor, model), attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		break;
	case VertexAttribute::JOINTS:
		if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
		{
			FillInterleavedBufferWithAttribute(interleavedBuffer, GetAccessorBytes(accessor, model), attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		}
		else
		{
			assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);

			// Convert from unsigned short to unsigned byte
			auto jointBytes = GetAccessorBytes(accessor, model);
			std::span<glm::u16vec4> joints((glm::u16vec4*)jointBytes.data(), accessor.count);
			std::vector<glm::u8vec4> jointsAsUnsignedBytes(accessor.count);
			std::transform(joints.begin(), joints.end(), jointsAsUnsignedBytes.begin(),
				[](glm::u16vec4 indices)
				{
					assert(indices.x < 255 && indices.y < 255 && indices.z < 255 && indices.w < 255);
					return glm::u8vec4(indices);
				});

			std::span<const std::uint8_t> attrBytes((std::uint8_t*)jointsAsUnsignedBytes.data(), sizeof(glm::u8vec4) * jointsAsUnsignedBytes.size());

			FillInterleavedBufferWithAttribute(interleavedBuffer, attrBytes, attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		}
		break;
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

	FillInterleavedBufferWithAttribute(buffer, positionsAccessor, vertexSizeBytes, VertexAttribute::POSITION, attributes, model);
	
	if (HasFlag(attributes, VertexAttribute::TEXCOORD))
	{
		int tcAccessorIndex = primitive.attributes.find("TEXCOORD_0")->second;
		const tinygltf::Accessor& tcAccessor = model.accessors[tcAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, tcAccessor, vertexSizeBytes, VertexAttribute::TEXCOORD, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::NORMAL))
	{
		int normalAccessorIndex = primitive.attributes.find("NORMAL")->second;
		const tinygltf::Accessor& normalsAccessor = model.accessors[normalAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, normalsAccessor, vertexSizeBytes, VertexAttribute::NORMAL, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::WEIGHTS))
	{
		assert(HasFlag(attributes, VertexAttribute::JOINTS));

		int weightsAccessorIndex = primitive.attributes.find("WEIGHTS_0")->second;
		const tinygltf::Accessor& weightsAccessor = model.accessors[weightsAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, weightsAccessor, vertexSizeBytes, VertexAttribute::WEIGHTS, attributes, model);

		int jointsAccessorIndex = primitive.attributes.find("JOINTS_0")->second;
		const tinygltf::Accessor& jointsAccessor = model.accessors[jointsAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, jointsAccessor, vertexSizeBytes, VertexAttribute::JOINTS, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_POSITION))
	{
		int accessorIndex = primitive.targets[0].find("POSITION")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET0_POSITION, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_POSITION))
	{
		int accessorIndex = primitive.targets[1].find("POSITION")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET1_POSITION, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_NORMAL))
	{
		int accessorIndex = primitive.targets[0].find("NORMAL")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET0_NORMAL, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_NORMAL))
	{
		int accessorIndex = primitive.targets[1].find("NORMAL")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET1_NORMAL, attributes, model);
	}

	return buffer;
}

static std::vector<std::uint32_t> GetIndexBuffer(const tinygltf::Primitive& primitive, const tinygltf::Model& model, int offset)
{
	const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
	auto indicesAccessorBytes = GetAccessorBytes(indicesAccessor, model);
	std::vector<std::uint32_t> indexBuffer(indicesAccessor.count);
	int componentSizeBytes = tinygltf::GetComponentSizeInBytes(indicesAccessor.componentType);

	if (componentSizeBytes == 4)
	{
		std::span<std::uint32_t> indicesAccessorData((std::uint32_t*)(indicesAccessorBytes.data()), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(), 
			[offset](std::uint32_t index) { return index + offset; });
	}
	else if (componentSizeBytes == 2)
	{
		std::span<std::uint16_t> indicesAccessorData((std::uint16_t*)(indicesAccessorBytes.data()), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(),
			[offset](std::uint16_t index) { return std::uint32_t(index) + offset; });
	}
	else
	{
		assert(componentSizeBytes == 1 && "Invalid index buffer component size.");
		std::span<std::uint8_t> indicesAccessorData((std::uint8_t*)(indicesAccessorBytes.data()), indicesAccessor.count);
		std::transform(indicesAccessorData.begin(), indicesAccessorData.end(), indexBuffer.begin(),
			[offset](std::uint8_t index) { return std::uint32_t(index) + offset; });
	}
	
	return indexBuffer;
}

// Create one interleaved vertex buffer that contains the vertices of all the mesh's primitives
Mesh::Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	assert(mesh.primitives.size() > 0);

	flags = GetPrimitiveVertexLayout(mesh.primitives[0]);
	bool hasJoints = HasFlag(flags, VertexAttribute::JOINTS);
	bool hasMorphTargets = HasFlag(flags, VertexAttribute::MORPH_TARGET0_POSITION);
	assert((!hasJoints && !hasMorphTargets) || (hasJoints != hasMorphTargets) && "Morph targets and skeletal animation on same mesh not supported");

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

		VertexAttribute primitiveFlags = GetPrimitiveVertexLayout(primitive);
		assert(flags == primitiveFlags && "All mesh primitives must have the same attributes");

		submeshes.emplace_back();
		Submesh& submesh = submeshes.back();
		std::vector<std::uint8_t> primitiveVertexBuffer = GetInterleavedVertexBuffer(primitive, primitiveFlags, model);
		int vertexBufferBytes = vertexBuffer.size();
		int countVertices = vertexBufferBytes / vertexSizeBytes;
		submesh.start = countVertices;
		int countPrimitiveVertices = primitiveVertexBuffer.size() / vertexSizeBytes;
		submesh.countVerticesOrIndices = countPrimitiveVertices;
		vertexBuffer.insert(vertexBuffer.end(), primitiveVertexBuffer.begin(), primitiveVertexBuffer.end());
		submesh.materialIndex = primitive.material;

		if (hasIndexBuffer)
		{
			std::vector<std::uint32_t> primitiveIndexBuffer = GetIndexBuffer(primitive, model, primitiveIndicesOffset);
			submesh.start = indexBuffer.size();
			submesh.countVerticesOrIndices = primitiveIndexBuffer.size();
			indexBuffer.insert(indexBuffer.end(), primitiveIndexBuffer.begin(), primitiveIndexBuffer.end());
			primitiveIndicesOffset += countVertices;
		}
	}

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	
	GLuint VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size(), vertexBuffer.data(), GL_STATIC_DRAW);

	// Don't change attribute indices, shaders rely on them being in this order

	// Position
	int offset = 0;
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
	offset += attributeByteSizes.find(VertexAttribute::POSITION)->second;

	if (HasFlag(flags, VertexAttribute::TEXCOORD))
	{
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::TEXCOORD)->second;
	}

	if (HasFlag(flags, VertexAttribute::NORMAL))
	{
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::NORMAL)->second;
	}

	if (HasFlag(flags, VertexAttribute::WEIGHTS))
	{
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::WEIGHTS)->second;

		glEnableVertexAttribArray(4);
		glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::JOINTS)->second;
	}

	if (HasFlag(flags, VertexAttribute::MORPH_TARGET0_POSITION))
	{
		assert(HasFlag(flags, VertexAttribute::MORPH_TARGET1_POSITION));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET0_POSITION)->second;

		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET1_POSITION)->second;
	}

	if (HasFlag(flags, VertexAttribute::MORPH_TARGET0_NORMAL))
	{
		assert(HasFlag(flags, VertexAttribute::MORPH_TARGET1_NORMAL));
		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET0_NORMAL)->second;

		glEnableVertexAttribArray(8);
		glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, vertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET1_NORMAL)->second;
	}
	
	if (hasIndexBuffer)
	{
		GLuint IBO;
		glGenBuffers(1, &IBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.size() * sizeof(indexBuffer[0]), indexBuffer.data(), GL_STATIC_DRAW);
	}
}