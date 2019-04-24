#include "Scene.h"
#include "Graphics/RenderSystem.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <memory>
#include "Utility/Utility.h"

void VEngine::Scene::load(RenderSystem &renderSystem, std::string filepath)
{
	nlohmann::json info;
	{
		std::ifstream file(filepath);
		file >> info;
	}

	uint32_t vertexAreaSize = info["VertexSize"];
	uint32_t indexAreaSize = info["IndexSize"];

	auto getTextureHandle = [&renderSystem, this](const std::string &filepath) -> uint32_t
	{
		if (filepath.empty())
		{
			return 0;
		}

		auto it = m_textures.find(filepath);
		if (it != m_textures.end())
		{
			return it->second;
		}
		else
		{
			uint32_t handle = renderSystem.createTexture(filepath.c_str());
			m_textures[filepath] = handle;
			return handle;
		}
	};

	std::vector<char> meshData = Utility::readBinaryFile(info["MeshFile"].get<std::string>().c_str());

	std::vector<Material> materials;
	std::vector<std::pair<SubMeshHandle, MaterialHandle>> mesh;
	for (auto &subMeshInfo : info["SubMeshes"])
	{
		uint32_t vertexOffset = subMeshInfo["VertexOffset"].get<uint32_t>();
		uint32_t vertexSize = subMeshInfo["VertexSize"].get<uint32_t>();
		uint8_t *vertexData = (uint8_t *)meshData.data() + vertexOffset;

		uint32_t indexOffset = subMeshInfo["IndexOffset"].get<uint32_t>() + vertexAreaSize;
		uint32_t indexCount = subMeshInfo["IndexSize"].get<uint32_t>() / 4;
		uint32_t *indexData = (uint32_t*)(meshData.data() + indexOffset);

		SubMeshHandle subMeshHandle;
		renderSystem.createSubMeshes(1, &vertexSize, &vertexData, &indexCount, &indexData, &subMeshHandle);
		

		Material material;
		material.m_alpha = Material::Alpha(subMeshInfo["Material"]["Alpha"]);
		material.m_albedoFactor = glm::vec3(subMeshInfo["Material"]["AlbedoFactor"][0], subMeshInfo["Material"]["AlbedoFactor"][1], subMeshInfo["Material"]["AlbedoFactor"][2]);
		material.m_metallicFactor = subMeshInfo["Material"]["MetallicFactor"];
		material.m_roughnessFactor = subMeshInfo["Material"]["RoughnessFactor"];
		material.m_emissiveFactor = glm::vec3(subMeshInfo["Material"]["EmissiveFactor"][0], subMeshInfo["Material"]["EmissiveFactor"][1], subMeshInfo["Material"]["EmissiveFactor"][2]);
		material.m_albedoTexture = getTextureHandle(subMeshInfo["Material"]["AlbedoTexture"].get<std::string>());
		material.m_normalTexture = getTextureHandle(subMeshInfo["Material"]["NormalTexture"].get<std::string>());
		material.m_metallicTexture = getTextureHandle(subMeshInfo["Material"]["MetallicTexture"].get<std::string>());
		material.m_roughnessTexture = getTextureHandle(subMeshInfo["Material"]["RoughnessTexture"].get<std::string>());
		material.m_occlusionTexture = getTextureHandle(subMeshInfo["Material"]["OcclusionTexture"].get<std::string>());
		material.m_emissiveTexture = getTextureHandle(subMeshInfo["Material"]["EmissiveTexture"].get<std::string>());
		material.m_displacementTexture = getTextureHandle(subMeshInfo["Material"]["DisplacementTexture"].get<std::string>());

		MaterialHandle materialHandle;
		renderSystem.createMaterials(1, &material, &materialHandle);

		mesh.push_back({ subMeshHandle, materialHandle });
	}

	m_meshes[filepath] = mesh;
	
	renderSystem.updateTextureData();
}

void VEngine::Scene::unload(RenderSystem &renderSystem)
{
}
