#include "GLTFResources.h"

#include <iostream>
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

static bool IsLinearSpaceTexture(int textureIdx, const std::vector<tinygltf::Material> materials)
{
	for (const tinygltf::Material& material : materials)
	{
		if (textureIdx == material.pbrMetallicRoughness.baseColorTexture.index || textureIdx == material.emissiveTexture.index)
		{
			return false;
		}
		else if (textureIdx == material.pbrMetallicRoughness.metallicRoughnessTexture.index || textureIdx == material.normalTexture.index || textureIdx == material.occlusionTexture.index)
		{
			return true;
		}
	}
	assert(false && "Should not be here");
	return false;
}

GLTFResources::GLTFResources(const tinygltf::Model& model)
{
	for (const auto& extension : model.extensionsUsed)
	{
		std::cout << extension << '\n';
	}

	for (const tinygltf::Mesh& mesh : model.meshes)
	{
		meshes.emplace_back(mesh, model);
	}

	for (int i = 0; i < model.textures.size(); i++)
	{
		const tinygltf::Texture& texture = model.textures[i];
		assert(texture.source >= 0);

		const tinygltf::Image& image = model.images[texture.source];
		bool linearSpaceTexture = IsLinearSpaceTexture(i, model.materials);

		int numComponents = image.component;
		GLenum internalFormat;
		GLenum format;
		if (numComponents == 1) {
			internalFormat = GL_RED;
			format = GL_RED;
		}
		if (numComponents == 2) {
			internalFormat = GL_RG;
			format = GL_RG;
		}
		else if (numComponents == 3) {
			internalFormat = linearSpaceTexture ? GL_RGB : GL_SRGB;
			format = GL_RGB;
		}
		else if (numComponents == 4) {
			internalFormat = linearSpaceTexture ? GL_RGBA : GL_SRGB_ALPHA;
			format = GL_RGBA;
		}
		else
		{
			std::cout << "Unsupported number of components: " << numComponents << " from file " << image.uri << '\n';
			std::exit(1);
		}

		textures.emplace_back();
		auto& addedTexture = textures.back();

		glGenTextures(1, &addedTexture.id);
		glBindTexture(GL_TEXTURE_2D, addedTexture.id);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, format, image.pixel_type, image.image.data());
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

	depth1x1Cubemap = textures.size();
	textures.emplace_back(Texture::DepthCubemap1x1());

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
	shaders.push_back({ { attributes, flatShading }, Shader("Shaders/default.vert", "Shaders/default.frag", nullptr, defines)});
	return shaders.back().second;
}

Shader& GLTFResources::GetOrCreateDepthShader(VertexAttribute attributes, bool depthCubemap)
{
	// Only take attributes that affect depth shading into account
	constexpr VertexAttribute depthShadingAttributes = VertexAttribute::POSITION | VertexAttribute::JOINTS | VertexAttribute::WEIGHTS | VertexAttribute::MORPH_TARGET0_POSITION;
	VertexAttribute relevantAttributes = attributes & depthShadingAttributes;
	for (auto& pair : depthShaders)
	{
		VertexAttribute key = (pair.first.first & depthShadingAttributes);
		if (relevantAttributes == key && depthCubemap == pair.first.second)
		{
			return pair.second;
		}
	}
	auto defines = GetShaderDefines(relevantAttributes, false);
	if (!depthCubemap)
	{
		depthShaders.push_back({ { relevantAttributes, false }, Shader("Shaders/depth.vert", "Shaders/empty.frag", nullptr, defines) });
	}
	else
	{
		depthShaders.push_back({ { relevantAttributes, true }, Shader("Shaders/transform.vert", "Shaders/empty.frag", "Shaders/cubedepth.geom", defines)});
	}
	return depthShaders.back().second;
}

Shader& GLTFResources::GetOrCreateHighlightShader(VertexAttribute attributes)
{
	constexpr VertexAttribute highlightAttributes = VertexAttribute::POSITION | VertexAttribute::JOINTS | VertexAttribute::WEIGHTS | VertexAttribute::MORPH_TARGET0_POSITION;
	VertexAttribute relevantAttributes = attributes & highlightAttributes;
	for (auto& pair : highlightShaders)
	{
		VertexAttribute key = (pair.first & highlightAttributes);
		if (relevantAttributes == key)
		{
			return pair.second;
		}
	}
	auto defines = GetShaderDefines(relevantAttributes, false);
	highlightShaders.push_back({ relevantAttributes, Shader("Shaders/transform.vert", "Shaders/highlight.frag", nullptr, defines) });
	return highlightShaders.back().second;
}