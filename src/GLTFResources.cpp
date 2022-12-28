#include "GLTFResources.h"

#include <glad/glad.h>
#include <string>
#include <tuple>
#include <vector>

static std::vector<std::string> GetShaderDefines(VertexAttribute flags)
{
	std::vector<std::string> defines;

	if (HasFlag(flags, VertexAttribute::TEXCOORD))
	{
		defines.emplace_back("HAS_TEXCOORD");
	}
	if (HasFlag(flags, VertexAttribute::NORMAL))
	{
		defines.emplace_back("HAS_NORMALS");
	}
	if (HasFlag(flags, VertexAttribute::JOINTS))
	{
		defines.emplace_back("HAS_JOINTS");
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

		// TODO: make shaders depend on materials as well
		if (!shaders.contains(addedMesh.flags))
		{
			auto defines = GetShaderDefines(addedMesh.flags);
			shaders.insert_or_assign(addedMesh.flags, Shader("Shaders/default.vert", "Shaders/default.frag", nullptr, {}, defines));
		}
	}

	for (const tinygltf::Texture& texture : model.textures)
	{
		const tinygltf::Image& image = model.images[texture.source];
		const tinygltf::Sampler& sampler = model.samplers[texture.sampler];

		int numComponents = image.component;
		GLenum format;
		if (numComponents == 1) format = GL_RED;
		if (numComponents == 2) format = GL_RG;
		else if (numComponents == 3) format = GL_RGB;
		else if (numComponents == 4) format = GL_RGBA;
		else
		{
			std::cout << "Unsupported number of components: " << numComponents << " from file " << image.uri << '\n';
			std::exit(1);
		}

		textures.emplace_back();
		auto& addedTexture = textures.back();
		
		glGenTextures(1, &addedTexture.id);
		glBindTexture(GL_TEXTURE_2D, addedTexture.id);
		glTexImage2D(GL_TEXTURE_2D, 0, format, image.width, image.height, 0, format, image.pixel_type, image.image.data());
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
		if (sampler.minFilter != -1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
		if (sampler.magFilter != -1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
	}
}

Shader& GLTFResources::GetMeshShader(const Mesh& mesh)
{
	auto iter = shaders.find(mesh.flags);
	assert(iter != shaders.end() && "No shader found for mesh");
	return iter->second;
}