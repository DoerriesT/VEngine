#include "Scene.h"
#include "Graphics/RenderSystem.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <memory>
#include "Utility/Utility.h"
#include <random>

void VEngine::Scene::load(RenderSystem &renderSystem, std::string filepath)
{
	nlohmann::json info;
	{
		std::ifstream file(filepath + ".info");
		file >> info;
	}

	// load material library
	{
		std::default_random_engine e;
		std::uniform_real_distribution<float> c(0.0f, 1.0f);
		auto getTextureHandle = [&renderSystem, this](const std::string &filepath) -> uint32_t
		{
			return 0;
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

		const std::string materialLibraryFileName = info["materialLibraryFile"].get<std::string>();

		nlohmann::json matlib;
		{
			std::ifstream file(materialLibraryFileName);
			file >> matlib;
		}

		auto &materialHandles = m_materials[materialLibraryFileName];

		std::vector<Material> materials;

		const uint32_t materialCount = static_cast<uint32_t>(matlib["materials"].size());
		materials.reserve(materialCount);
		materialHandles.resize(materialCount);

		for (const auto &mat : matlib["materials"])
		{
			Material material;
			material.m_alpha = Material::Alpha(mat["alphaMode"].get<uint32_t>());
			material.m_albedoFactor = glm::vec3(c(e), c(e), c(e));// glm::vec3(mat["albedo"][0], mat["albedo"][1], mat["albedo"][2]);
			material.m_metallicFactor = mat["metalness"];
			material.m_roughnessFactor = mat["roughness"];
			material.m_emissiveFactor = glm::vec3(mat["emissive"][0], mat["emissive"][1], mat["emissive"][2]);
			material.m_albedoTexture = getTextureHandle(mat["albedoTexture"].get<std::string>());
			material.m_normalTexture = getTextureHandle(mat["normalTexture"].get<std::string>());
			material.m_metallicTexture = getTextureHandle(mat["metalnessTexture"].get<std::string>());
			material.m_roughnessTexture = getTextureHandle(mat["roughnessTexture"].get<std::string>());
			material.m_occlusionTexture = getTextureHandle(mat["occlusionTexture"].get<std::string>());
			material.m_emissiveTexture = getTextureHandle(mat["emissiveTexture"].get<std::string>());
			material.m_displacementTexture = getTextureHandle(mat["displacementTexture"].get<std::string>());

			materials.push_back(material);
		}

		renderSystem.createMaterials(materials.size(), materials.data(), materialHandles.data());
		renderSystem.updateTextureData();
	}

	// load mesh
	{
		const uint32_t vertexAreaSize = info["vertexSize"];
		const uint32_t indexAreaSize = info["indexSize"];

		const std::vector<char> meshData = Utility::readBinaryFile(info["meshFile"].get<std::string>().c_str());

		const uint32_t subMeshCount = static_cast<uint32_t>(info["subMeshes"].size());
		std::vector<uint32_t> vertexSizes;
		std::vector<uint8_t*> vertexData;
		std::vector<uint32_t> indexCounts;
		std::vector<uint32_t*> indexData;
		std::vector<AxisAlignedBoundingBox> aabbs;
		auto &subMeshHandles = m_meshes[filepath];

		vertexSizes.reserve(subMeshCount);
		vertexData.reserve(subMeshCount);
		indexCounts.reserve(subMeshCount);
		indexData.reserve(subMeshCount);
		aabbs.reserve(subMeshCount);
		subMeshHandles.resize(subMeshCount);

		for (auto &subMeshInfo : info["subMeshes"])
		{
			uint32_t vOffset = subMeshInfo["vertexOffset"].get<uint32_t>();
			uint32_t vSize = subMeshInfo["vertexSize"].get<uint32_t>();
			uint8_t *vData = (uint8_t *)meshData.data() + vOffset;

			uint32_t iOffset = subMeshInfo["indexOffset"].get<uint32_t>() + vertexAreaSize;
			uint32_t iCount = subMeshInfo["indexSize"].get<uint32_t>() / 4;
			uint32_t *iData = (uint32_t*)(meshData.data() + iOffset);

			AxisAlignedBoundingBox aabb =
			{
				glm::vec3(subMeshInfo["minCorner"][0], subMeshInfo["minCorner"][1], subMeshInfo["minCorner"][2]),
				glm::vec3(subMeshInfo["maxCorner"][0], subMeshInfo["maxCorner"][1], subMeshInfo["maxCorner"][2])
			};

			vertexSizes.push_back(vSize);
			vertexData.push_back(vData);
			indexCounts.push_back(iCount);
			indexData.push_back(iData);
			aabbs.push_back(aabb);
		}

		renderSystem.createSubMeshes(subMeshCount, vertexSizes.data(), vertexData.data(), indexCounts.data(), indexData.data(), aabbs.data(), subMeshHandles.data());
	}

	// create submesh instances
	{
		auto &subMeshHandles = m_meshes[filepath];
		auto &materialHandles = m_materials[info["materialLibraryFile"].get<std::string>()];
		auto &instances = m_meshInstances[filepath];

		for (auto &subMeshInstance : info["subMeshInstances"])
		{
			uint32_t subMeshIndex = subMeshInstance["subMesh"].get<uint32_t>();
			uint32_t materialIndex = subMeshInstance["material"].get<uint32_t>();
			instances.push_back({ subMeshHandles[subMeshIndex], materialHandles[materialIndex] });
		}
	}
}

void VEngine::Scene::unload(RenderSystem &renderSystem)
{
}
