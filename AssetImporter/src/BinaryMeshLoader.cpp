#include "BinaryMeshLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "Utility.h"

Model BinaryMeshLoader::loadModel(const std::string &filepath, bool mergeByMaterial, bool invertTexcoordY) const
{
	Model resultModel;

	nlohmann::json info;
	{
		std::ifstream file(filepath + ".info");
		file >> info;
	}

	// load material library
	{
		const std::string materialLibraryFileName = info["materialLibraryFile"].get<std::string>();

		nlohmann::json matlib;
		{
			std::ifstream file(materialLibraryFileName);
			file >> matlib;
		}

		const uint32_t materialCount = static_cast<uint32_t>(matlib["materials"].size());
		resultModel.m_materials.reserve(materialCount);

		for (const auto &mat : matlib["materials"])
		{
			Material material;
			material.m_name = mat["name"].get<std::string>();
			material.m_alpha = Material::Alpha(mat["alphaMode"].get<uint32_t>());
			material.m_albedoFactor = glm::vec3(mat["albedo"][0], mat["albedo"][1], mat["albedo"][2]);
			material.m_metalnessFactor = mat["metalness"];
			material.m_roughnessFactor = mat["roughness"];
			material.m_emissiveFactor = glm::vec3(mat["emissive"][0], mat["emissive"][1], mat["emissive"][2]);
			material.m_opacity = mat["opacity"].get<float>();
			material.m_albedoTexture = mat["albedoTexture"].get<std::string>();
			material.m_normalTexture = mat["normalTexture"].get<std::string>();
			material.m_metalnessTexture = mat["metalnessTexture"].get<std::string>();
			material.m_roughnessTexture = mat["roughnessTexture"].get<std::string>();
			material.m_occlusionTexture = mat["occlusionTexture"].get<std::string>();
			material.m_emissiveTexture = mat["emissiveTexture"].get<std::string>();
			material.m_displacementTexture = mat["displacementTexture"].get<std::string>();

			resultModel.m_materials.push_back(material);
		}
	}

	// load mesh
	{
		resultModel.m_aabbMin = { info["minCorner"][0].get<float>(), info["minCorner"][1].get<float>(), info["minCorner"][2].get<float>() };
		resultModel.m_aabbMax = { info["maxCorner"][0].get<float>(), info["maxCorner"][1].get<float>(), info["maxCorner"][2].get<float>() };

		const std::vector<char> meshData = Utility::readBinaryFile(info["meshFile"].get<std::string>().c_str());

		const uint32_t subMeshCount = static_cast<uint32_t>(info["subMeshes"].size());
		resultModel.m_meshes.reserve(subMeshCount);

		for (auto &subMeshInfo : info["subMeshes"])
		{
			size_t dataOffset = subMeshInfo["dataOffset"].get<size_t>();

			Mesh subMesh;
			subMesh.m_name = subMeshInfo["name"].get<std::string>();
			subMesh.m_aabbMin = { subMeshInfo["minCorner"][0].get<float>(), subMeshInfo["minCorner"][1].get<float>(), subMeshInfo["minCorner"][2].get<float>() };
			subMesh.m_aabbMax = { subMeshInfo["maxCorner"][0].get<float>(), subMeshInfo["maxCorner"][1].get<float>(), subMeshInfo["maxCorner"][2].get<float>() };
			
			uint32_t vertexCount = subMeshInfo["vertexCount"].get<uint32_t>();
			uint32_t indexCount = subMeshInfo["indexCount"].get<uint32_t>();

			glm::vec3 *positions = (glm::vec3 *)((uint8_t *)meshData.data() + dataOffset);
			dataOffset += vertexCount * sizeof(glm::vec3);

			glm::vec3 *normals = (glm::vec3 *)((uint8_t *)meshData.data() + dataOffset);
			dataOffset += vertexCount * sizeof(glm::vec3);

			glm::vec2 *texCoords = (glm::vec2 *)((uint8_t *)meshData.data() + dataOffset);
			dataOffset += vertexCount * sizeof(glm::vec2);

			uint16_t *indices = (uint16_t *)(meshData.data() + dataOffset);

			for (size_t i = 0; i < indexCount; ++i)
			{
				uint16_t index = indices[i];
				subMesh.m_positions.push_back(positions[index]);
				subMesh.m_normals.push_back(normals[index]);
				subMesh.m_texCoords.push_back(texCoords[index]);
			}
			subMesh.m_tangents.resize(subMesh.m_positions.size());

			resultModel.m_meshes.push_back(subMesh);
		}

		// create submesh instances
		{
			for (auto &subMeshInstance : info["subMeshInstances"])
			{
				uint32_t subMeshIndex = subMeshInstance["subMesh"].get<uint32_t>();
				uint32_t materialIndex = subMeshInstance["material"].get<uint32_t>();
				resultModel.m_meshes[subMeshIndex].m_materialIndex = materialIndex;
			}
		}
	}

	return resultModel;
}
