#include "Scene.h"
#include "Graphics/RenderSystem.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <memory>
#include "Utility/Utility.h"
#include <random>
#include <set>

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
		const std::vector<char> meshData = Utility::readBinaryFile(info["meshFile"].get<std::string>().c_str());

		const uint32_t subMeshCount = static_cast<uint32_t>(info["subMeshes"].size());
		std::vector<SubMesh> subMeshes;
		auto &subMeshHandles = m_meshes[filepath];

		subMeshes.reserve(subMeshCount);
		subMeshHandles.resize(subMeshCount);

		for (auto &subMeshInfo : info["subMeshes"])
		{
			size_t dataOffset = subMeshInfo["dataOffset"].get<size_t>();

			SubMesh subMesh;
			subMesh.m_minCorner = { subMeshInfo["minCorner"][0], subMeshInfo["minCorner"][1], subMeshInfo["minCorner"][2] };
			subMesh.m_maxCorner = { subMeshInfo["maxCorner"][0], subMeshInfo["maxCorner"][1], subMeshInfo["maxCorner"][2] };
			subMesh.m_vertexCount = subMeshInfo["vertexCount"].get<uint32_t>();
			subMesh.m_indexCount = subMeshInfo["indexCount"].get<uint32_t>();

			subMesh.m_positions = (uint8_t *)meshData.data() + dataOffset;
			dataOffset += subMesh.m_vertexCount * sizeof(glm::vec3);

			subMesh.m_normals = (uint8_t *)meshData.data() + dataOffset;
			dataOffset += subMesh.m_vertexCount * sizeof(glm::vec3);

			subMesh.m_texCoords = (uint8_t *)meshData.data() + dataOffset;
			dataOffset += subMesh.m_vertexCount * sizeof(glm::vec2);

			subMesh.m_indices = (uint32_t*)(meshData.data() + dataOffset);

			subMeshes.push_back(subMesh);
		}

		renderSystem.createSubMeshes(subMeshCount, subMeshes.data(), subMeshHandles.data());
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
