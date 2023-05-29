#include "PBRMaterial.h"

PBRMaterial FromGltfMaterial(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, int white1x1RGBATextureIndex, int max1x1RedTextureIndex)
{
	static int defaultMaterialNameSuffix = 0;
	const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;
	assert(pbr.baseColorTexture.texCoord == 0 && 
		   pbr.metallicRoughnessTexture.texCoord == 0 && 
		   gltfMaterial.normalTexture.texCoord == 0 && 
		   gltfMaterial.occlusionTexture.texCoord == 0 && 
		   "Multiple tex coords not currently supported");

	assert(pbr.baseColorTexture.index < 0 || model.images[model.textures[pbr.baseColorTexture.index].source].component == 4 && "Assuming RGBA for base color texture");

	PBRMaterial material;
	material.name = gltfMaterial.name;
	if (material.name.empty())
	{
		material.name = "Material" + defaultMaterialNameSuffix;
		defaultMaterialNameSuffix++;
	}
	material.baseColorFactor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
	material.baseColorTextureIdx = pbr.baseColorTexture.index;
	if (material.baseColorTextureIdx < 0)
	{
		material.baseColorTextureIdx = white1x1RGBATextureIndex;
	}
	material.metallicFactor = (float)pbr.metallicFactor;
	material.roughnessFactor = (float)pbr.roughnessFactor;
	material.metallicRoughnessTextureIdx = pbr.metallicRoughnessTexture.index;
	if (material.metallicRoughnessTextureIdx < 0)
	{
		material.metallicRoughnessTextureIdx = white1x1RGBATextureIndex;
	}
	material.normalTextureIdx = gltfMaterial.normalTexture.index;
	material.normalScale = gltfMaterial.normalTexture.scale;
	//assert(gltfMaterial.normalTexture.index < 0 || gltfMaterial.normalTexture.scale == 1.0f);

	material.occlusionStrength = gltfMaterial.occlusionTexture.strength;
	material.occlusionTextureIdx = gltfMaterial.occlusionTexture.index;
	if (material.occlusionTextureIdx < 0)
	{
		material.occlusionTextureIdx = max1x1RedTextureIndex;
	}

	return material;
}