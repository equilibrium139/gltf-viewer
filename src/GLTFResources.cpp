#include "GLTFResources.h"

#include <glad/glad.h>
#include <string>
#include <tuple>
#include <vector>

static std::vector<std::string> GetShaderDefines(VertexAttribute flags, bool flatShading)
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
	if (flatShading)
	{
		defines.emplace_back("FLAT_SHADING");
	}
	if (HasFlag(flags, VertexAttribute::TANGENT))
	{
		defines.emplace_back("HAS_TANGENTS");
	}
	if (HasFlag(flags, VertexAttribute::COLOR))
	{
		defines.emplace_back("HAS_VERTEX_COLORS");
	}


	return defines;
}

GLTFResources::GLTFResources(const tinygltf::Model& model)
{
	for (const tinygltf::Mesh& mesh : model.meshes)
	{
		meshes.emplace_back(mesh, model);
	}

	for (const tinygltf::Texture& texture : model.textures)
	{
		assert(texture.source >= 0);

		const tinygltf::Image& image = model.images[texture.source];

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

		if (texture.sampler >= 0)
		{
			const tinygltf::Sampler& sampler = model.samplers[texture.sampler];
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
			if (sampler.minFilter != -1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
			if (sampler.magFilter != -1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
		}
	}

	white1x1RGBAIndex = textures.size();
	textures.emplace_back(Texture::White1x1TextureRGBA());

	max1x1RedIndex = textures.size();
	textures.emplace_back(Texture::Max1x1TextureRed());

	for (const tinygltf::Material& gltfMaterial : model.materials)
	{
		materials.emplace_back(FromGltfMaterial(gltfMaterial, model, white1x1RGBAIndex, max1x1RedIndex));
	}
}

Shader& GLTFResources::GetOrCreateShader(VertexAttribute attributes, bool flatShading)
{
	for (auto& pair : shaders)
	{
		const ShaderKey& key = pair.first;
		if (key.first == attributes && key.second == flatShading)
		{
			return pair.second;
		}
	}
	auto defines = GetShaderDefines(attributes, flatShading);
	shaders.push_back({ { attributes, flatShading }, Shader("Shaders/default.vert", "Shaders/default.frag", nullptr, {}, defines) });
	return shaders.back().second;
}

Shader& GLTFResources::GetOrCreateHighlightShader(VertexAttribute attributes)
{
	for (auto& pair : highlightShaders)
	{
		if (attributes == pair.first)
		{
			return pair.second;
		}
	}
	auto defines = GetShaderDefines(attributes, false);
	highlightShaders.push_back({ attributes, Shader("Shaders/default.vert", "Shaders/highlight.frag", nullptr, {}, defines) });
	return highlightShaders.back().second;
}