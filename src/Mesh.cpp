#include "Mesh.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include "GLTFHelpers.h"
#include "mikktspace.h"
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
	{"WEIGHTS_0", VertexAttribute::JOINTS},
	{"TANGENT", VertexAttribute::TANGENT },
	{"COLOR_0", VertexAttribute::COLOR },
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
	VertexAttribute::MORPH_TARGET1_NORMAL,
	VertexAttribute::TANGENT,
	VertexAttribute::MORPH_TARGET0_TANGENT,
	VertexAttribute::MORPH_TARGET1_TANGENT,
	VertexAttribute::COLOR,
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
	{VertexAttribute::MORPH_TARGET1_NORMAL, 12},
	{VertexAttribute::TANGENT, 16},
	{VertexAttribute::MORPH_TARGET0_TANGENT, 12},
	{VertexAttribute::MORPH_TARGET1_TANGENT, 12},
	{VertexAttribute::COLOR, 16}, // vertexColor is always converted to RGBA
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
		if (HasFlag(attributes, VertexAttribute::TANGENT))
		{
			assert(primitive.targets[0].contains("TANGENT") && primitive.targets[1].contains("TANGENT"));
			attributes |= VertexAttribute::MORPH_TARGET0_TANGENT;
			attributes |= VertexAttribute::MORPH_TARGET1_TANGENT;
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

static void FillInterleavedBufferWithAttribute(std::vector<std::uint8_t>& interleavedBuffer, std::span<const std::uint8_t> attrData, int attrSizeBytes, int attrOffset, 
	int vertexSizeBytes, int numVertices)
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

static void FillInterleavedBufferWithAttribute(std::vector<std::uint8_t>& interleavedBuffer, const tinygltf::Accessor& accessor, int vertexSizeBytes, VertexAttribute attribute,
	VertexAttribute attributes, const tinygltf::Model& model)
{
	// Positions, normals, and tangents are always float vec3 so we can always treat them the same, but the other types can have different component types
	// so they need to be converted to a single type
	switch (attribute)
	{
	case VertexAttribute::POSITION: case VertexAttribute::NORMAL: 
	case VertexAttribute::MORPH_TARGET0_POSITION: case VertexAttribute::MORPH_TARGET1_POSITION:
	case VertexAttribute::MORPH_TARGET0_NORMAL: case VertexAttribute::MORPH_TARGET1_NORMAL: 
	case VertexAttribute::TANGENT: case VertexAttribute::MORPH_TARGET0_TANGENT: case VertexAttribute::MORPH_TARGET1_TANGENT: 
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
	case VertexAttribute::COLOR:
		assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
		if (accessor.type == TINYGLTF_TYPE_VEC3) // convert to vec4
		{
			auto colorBytes = GetAccessorBytes(accessor, model);
			std::span<glm::vec3> rgbColors((glm::vec3*)colorBytes.data(), accessor.count);
			std::vector<glm::vec4> rgbaColors(accessor.count);
			for (int i = 0; i < accessor.count; i++)
			{
				rgbaColors[i] = glm::vec4(rgbColors[i], 1.0f);
			}
			std::span<const std::uint8_t> rgbaBytes((std::uint8_t*)rgbaColors.data(), sizeof(glm::vec4) * accessor.count);
			FillInterleavedBufferWithAttribute(interleavedBuffer, rgbaBytes, attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		}
		else // format is already vec4
		{
			FillInterleavedBufferWithAttribute(interleavedBuffer, GetAccessorBytes(accessor, model), attributeByteSizes.find(attribute)->second, GetAttributeByteOffset(attributes, attribute), vertexSizeBytes, accessor.count);
		}
	}
}

static std::vector<std::uint8_t> GetInterleavedVertexBuffer(const tinygltf::Primitive& primitive, VertexAttribute attributes, const tinygltf::Model& model, bool generateTangents)
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
	if (HasFlag(attributes, VertexAttribute::TANGENT) && !generateTangents)
	{
		int tangentAccessorIndex = primitive.attributes.find("TANGENT")->second;
		const tinygltf::Accessor& tangentAccessor = model.accessors[tangentAccessorIndex];
		FillInterleavedBufferWithAttribute(buffer, tangentAccessor, vertexSizeBytes, VertexAttribute::TANGENT, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET0_TANGENT))
	{
		int accessorIndex = primitive.targets[0].find("TANGENT")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET0_TANGENT, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::MORPH_TARGET1_TANGENT))
	{
		int accessorIndex = primitive.targets[1].find("TANGENT")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::MORPH_TARGET1_TANGENT, attributes, model);
	}
	if (HasFlag(attributes, VertexAttribute::COLOR))
	{
		int accessorIndex = primitive.attributes.find("COLOR_0")->second;
		const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
		FillInterleavedBufferWithAttribute(buffer, accessor, vertexSizeBytes, VertexAttribute::COLOR, attributes, model);
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

static BBox ComputeBoundingBox(const std::vector<std::uint8_t>& vertexBuffer, int stride)
{
	// assumes positions are at offset 0
	BBox bbox{
		.minXYZ = glm::vec3(FLT_MAX),
		.maxXYZ = glm::vec3(-FLT_MAX)
	};

	const std::uint8_t* vertexBufferPtr = vertexBuffer.data();
	const std::uint8_t* vertexBufferPtrEnd = vertexBuffer.data() + vertexBuffer.size();
	while (vertexBufferPtr < vertexBufferPtrEnd)
	{
		const glm::vec3* pos = reinterpret_cast<const glm::vec3*>(vertexBufferPtr);

		bbox.minXYZ = glm::min(*pos, bbox.minXYZ);
		bbox.maxXYZ = glm::max(*pos, bbox.maxXYZ);

		vertexBufferPtr += stride;
	}

	return bbox;
}

static void GenerateTangents(std::vector<std::uint8_t>& vertexBuffer, const std::vector<std::uint32_t>* indexBuffer, VertexAttribute attributes)
{
	assert(HasFlag(attributes, VertexAttribute::NORMAL | VertexAttribute::TEXCOORD | VertexAttribute::TANGENT) && "Must have normals and texture coordinates to generate tangents");

	SMikkTSpaceInterface mikktInterface{};

	SMikkTSpaceContext context{};
	struct UserData
	{
		std::vector<std::uint8_t>& vb; 
		const std::vector<std::uint32_t>* ib; 
		VertexAttribute attributes;
		int stride;
		int texcoordOffsetBytes;
		int normalOffsetBytes;
		int tangentOffsetBytes;
	};
	UserData userData{ vertexBuffer, indexBuffer, attributes, GetVertexSizeBytes(attributes), 
					   GetAttributeByteOffset(attributes, VertexAttribute::TEXCOORD), GetAttributeByteOffset(attributes, VertexAttribute::NORMAL),
					   GetAttributeByteOffset(attributes, VertexAttribute::TANGENT) };
	context.m_pUserData = &userData;
	context.m_pInterface = &mikktInterface;

	mikktInterface.m_getNumFaces = 
	[](const SMikkTSpaceContext* pContext) 
	{
		auto userData = static_cast<UserData*>(pContext->m_pUserData);
		if (userData->ib != nullptr) return (int)userData->ib->size() / 3;
		return (int)userData->vb.size() / 3;
	};
	mikktInterface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; }; // Assuming triangles
	if (indexBuffer != nullptr)
	{
		mikktInterface.m_getPosition =
		[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			std::uint32_t index = (*userData->ib)[iFace * 3 + iVert];
			const glm::vec3* pos = reinterpret_cast<const glm::vec3*>(&userData->vb[index * userData->stride]);
			fvPosOut[0] = pos->x;
			fvPosOut[1] = pos->y;
			fvPosOut[2] = pos->z;
		};

		mikktInterface.m_getNormal =
			[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			std::uint32_t index = (*userData->ib)[iFace * 3 + iVert];
			const glm::vec3* normal = reinterpret_cast<const glm::vec3*>(&userData->vb[(index * userData->stride) + userData->normalOffsetBytes]);
			fvPosOut[0] = normal->x;
			fvPosOut[1] = normal->y;
			fvPosOut[2] = normal->z;
		};

		mikktInterface.m_getTexCoord =
			[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			std::uint32_t index = (*userData->ib)[iFace * 3 + iVert];
			const glm::vec2* texCoord = reinterpret_cast<const glm::vec2*>(&userData->vb[(index * userData->stride) + userData->texcoordOffsetBytes]);
			fvPosOut[0] = texCoord->x;
			fvPosOut[1] = texCoord->y;
		};

		mikktInterface.m_setTSpaceBasic =
			[](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			std::uint32_t index = (*userData->ib)[iFace * 3 + iVert];
			glm::vec4* tangent = reinterpret_cast<glm::vec4*>(&userData->vb[(index * userData->stride) + userData->tangentOffsetBytes]);
			tangent->x = fvTangent[0];
			tangent->y = fvTangent[1];
			tangent->z = fvTangent[2];
			tangent->w = fSign;
		};
	}
	else
	{
		mikktInterface.m_getPosition =
			[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			int index = iFace * 3 + iVert;
			const glm::vec3* pos = reinterpret_cast<const glm::vec3*>(&userData->vb[index * userData->stride]);
			fvPosOut[0] = pos->x;
			fvPosOut[1] = pos->y;
			fvPosOut[2] = pos->z;
		};

		mikktInterface.m_getNormal =
			[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			int index = iFace * 3 + iVert;
			const glm::vec3* normal = reinterpret_cast<const glm::vec3*>(&userData->vb[(index * userData->stride) + userData->normalOffsetBytes]);
			fvPosOut[0] = normal->x;
			fvPosOut[1] = normal->y;
			fvPosOut[2] = normal->z;
		};

		mikktInterface.m_getTexCoord =
			[](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			int index = iFace * 3 + iVert;
			const glm::vec2* texCoord = reinterpret_cast<const glm::vec2*>(&userData->vb[(index * userData->stride) + userData->texcoordOffsetBytes]);
			fvPosOut[0] = texCoord->x;
			fvPosOut[1] = texCoord->y;
		};

		mikktInterface.m_setTSpaceBasic =
			[](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
		{
			auto userData = static_cast<const UserData*>(pContext->m_pUserData);
			int index = iFace * 3 + iVert;
			glm::vec4* tangent = reinterpret_cast<glm::vec4*>(&userData->vb[(index * userData->stride) + userData->tangentOffsetBytes]);
			tangent->x = fvTangent[0];
			tangent->y = fvTangent[1];
			tangent->z = fvTangent[2];
			tangent->w = fSign;
		};
	}

	genTangSpaceDefault(&context);
}

// Create one interleaved vertex buffer that contains the vertices of all the mesh's primitives
Mesh::Mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	assert(mesh.primitives.size() > 0);

	for (const tinygltf::Primitive& primitive : mesh.primitives)
	{
		assert(primitive.mode == GL_TRIANGLES);

		submeshes.emplace_back();
		Submesh& submesh = submeshes.back();

		submesh.flags = GetPrimitiveVertexLayout(primitive);
		bool hasJoints = HasFlag(submesh.flags, VertexAttribute::JOINTS);
		bool hasMorphTargets = HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION);
		assert((!hasJoints && !hasMorphTargets) || (hasJoints != hasMorphTargets) && "Morph targets and skeletal animation on same mesh not supported");

		submesh.materialIndex = primitive.material;
		bool hasMaterial = submesh.materialIndex >= 0;
		bool hasNormals = HasFlag(submesh.flags, VertexAttribute::NORMAL);
		submesh.flatShading = hasMaterial && !hasNormals;

		bool hasTangents = HasFlag(submesh.flags, VertexAttribute::TANGENT);
		assert(!hasTangents || hasNormals && "Primitive with tangents must also has normals");
		
		bool hasNormalMap = primitive.material >= 0 && model.materials[primitive.material].normalTexture.index >= 0;
		bool generateTangents = !hasTangents && hasNormalMap;
		if (generateTangents)
		{
			// TODO: generate tangents for morph targets
			assert(!hasMorphTargets && "Generating tangents with morph targets not currently supported");
			submesh.flags |= VertexAttribute::TANGENT;
		}

		bool discardTangents = hasTangents && !hasNormalMap; // wtf is the point?
		if (discardTangents)
		{
			submesh.flags &= ~VertexAttribute::TANGENT;
		}

		int submeshVertexSizeBytes = GetVertexSizeBytes(submesh.flags);
		std::vector<std::uint8_t> submeshVertexBuffer = GetInterleavedVertexBuffer(primitive, submesh.flags, model, generateTangents);

		submesh.hasIndexBuffer = primitive.indices >= 0;
		std::vector<std::uint32_t> primitiveIndexBuffer;
		if (submesh.hasIndexBuffer)
		{
			primitiveIndexBuffer = GetIndexBuffer(primitive, model, 0);
			submesh.countVerticesOrIndices = primitiveIndexBuffer.size();
		}
		else
		{
			submesh.countVerticesOrIndices = submeshVertexBuffer.size() / submeshVertexSizeBytes;
		}

		if (generateTangents)
		{
			GenerateTangents(submeshVertexBuffer, submesh.hasIndexBuffer ? &primitiveIndexBuffer : nullptr, submesh.flags);
		}

		BBox submeshBoundingBox = ComputeBoundingBox(submeshVertexBuffer, submeshVertexSizeBytes);
	
		boundingBox.minXYZ = glm::min(submeshBoundingBox.minXYZ, boundingBox.minXYZ);
		boundingBox.maxXYZ = glm::max(submeshBoundingBox.maxXYZ, boundingBox.maxXYZ);

		glGenVertexArrays(1, &submesh.VAO);
		glBindVertexArray(submesh.VAO);

		GLuint VBO;
		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, submeshVertexBuffer.size(), submeshVertexBuffer.data(), GL_STATIC_DRAW);

		// Don't change attribute indices, shaders rely on them being in this order

		// Position
		int offset = 0;
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
		offset += attributeByteSizes.find(VertexAttribute::POSITION)->second;

		if (HasFlag(submesh.flags, VertexAttribute::TEXCOORD))
		{
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::TEXCOORD)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::NORMAL))
		{
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::NORMAL)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::WEIGHTS))
		{
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::WEIGHTS)->second;

			glEnableVertexAttribArray(4);
			glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::JOINTS)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION))
		{
			assert(HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET1_POSITION));
			glEnableVertexAttribArray(5);
			glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET0_POSITION)->second;

			glEnableVertexAttribArray(6);
			glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET1_POSITION)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_NORMAL))
		{
			assert(HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET1_NORMAL));
			glEnableVertexAttribArray(7);
			glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET0_NORMAL)->second;

			glEnableVertexAttribArray(8);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET1_NORMAL)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::TANGENT))
		{
			glEnableVertexAttribArray(9);
			glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::TANGENT)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_TANGENT))
		{
			assert(HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET1_TANGENT));
			glEnableVertexAttribArray(10);
			glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET0_TANGENT)->second;

			glEnableVertexAttribArray(11);
			glVertexAttribPointer(11, 3, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::MORPH_TARGET1_TANGENT)->second;
		}

		if (HasFlag(submesh.flags, VertexAttribute::COLOR))
		{
			glEnableVertexAttribArray(12);
			glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, submeshVertexSizeBytes, (const void*)offset);
			offset += attributeByteSizes.find(VertexAttribute::COLOR)->second;
		}

		if (submesh.hasIndexBuffer)
		{
			GLuint IBO;
			glGenBuffers(1, &IBO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, primitiveIndexBuffer.size() * sizeof(primitiveIndexBuffer[0]), primitiveIndexBuffer.data(), GL_STATIC_DRAW);
		}
	}
}

bool Mesh::HasMorphTargets()
{
	for (Submesh& submesh : submeshes)
	{
		if (HasFlag(submesh.flags, VertexAttribute::MORPH_TARGET0_POSITION))
		{
			return true;
		}
	}
	return false;
}
