#include "GLTFResources.h"

#include <string>
#include <tuple>
#include <vector>

static std::vector<std::string> GetShaderDefines(VertexAttribute flags)
{
	std::vector<std::string> defines;

	if (HasFlag(flags, VertexAttribute::NORMAL))
	{
		defines.emplace_back("HAS_NORMALS");
	}
	if (HasFlag(flags, VertexAttribute::MORPH_TARGET0_POSITION))
	{
		defines.emplace_back("HAS_MORPH_TARGETS");
	}

	return defines;
}

GLTFResources::GLTFResources(const tinygltf::Model& model)
{
	for (const tinygltf::Mesh& mesh : model.meshes)
	{
		meshes.emplace_back(mesh, model);

		auto& addedMesh = meshes.back();
		if (!shaders.contains(addedMesh.flags))
		{
			auto defines = GetShaderDefines(addedMesh.flags);
			shaders.insert_or_assign(addedMesh.flags, Shader("Shaders/default.vert", "Shaders/default.frag", nullptr, {}, defines));
		}
	}
}

Shader& GLTFResources::GetMeshShader(const Mesh& mesh)
{
	auto iter = shaders.find(mesh.flags);
	assert(iter != shaders.end() && "No shader found for mesh");
	return iter->second;
}