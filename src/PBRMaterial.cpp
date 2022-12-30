#include "PBRMaterial.h"

PBRMaterial FromGltfMaterial(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model)
{
	static int defaultMaterialNameSuffix = 0;
	const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;
	assert(pbr.baseColorTexture.texCoord == 0 && pbr.metallicRoughnessTexture.texCoord == 0 && "Multiple tex coords not currently supported");

	PBRMaterial material;
	material.name = gltfMaterial.name;
	if (material.name.empty())
	{
		material.name = "Material" + defaultMaterialNameSuffix;
		defaultMaterialNameSuffix++;
	}
	material.baseColorFactor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
	material.baseColorTextureIdx = pbr.baseColorTexture.index;
	material.metallicFactor = (float)pbr.metallicFactor;
	material.roughnessFactor = (float)pbr.roughnessFactor;
	material.metallicRoughnessTextureIdx = pbr.metallicRoughnessTexture.index;
	material.normalTextureIdx = material.normalTextureIdx;

	return material;
}