#include <iostream>
#include <cassert>
#include <vector>
#include <glm\vec3.hpp>
#include <glm\vec2.hpp>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <iomanip>
#include "meshoptimizer-0.12/meshoptimizer.h"

#include "Utility.h"
#include "WavefrontOBJLoader.h"

void createMaterialLibrary(const std::vector<Material> &materials, const std::string dstFilePath)
{
	nlohmann::json j;

	j["count"] = materials.size();
	j["materials"] = nlohmann::json::array();

	for (const auto &mat : materials)
	{
		j["materials"].push_back(
			{
				{ "name", mat.m_name },
				{ "alphaMode", 0 },
				{ "albedo", { mat.m_albedoFactor[0], mat.m_albedoFactor[1], mat.m_albedoFactor[2] } },
				{ "metalness", mat.m_metalnessFactor },
				{ "roughness", mat.m_roughnessFactor },
				{ "emissive", { mat.m_emissiveFactor[0], mat.m_emissiveFactor[1], mat.m_emissiveFactor[2] } },
				{ "opacity", mat.m_opacity },
				{ "albedoTexture", mat.m_albedoTexture },
				{ "normalTexture", mat.m_normalTexture },
				{ "metalnessTexture", mat.m_metalnessTexture },
				{ "roughnessTexture", mat.m_roughnessTexture },
				{ "occlusionTexture", mat.m_occlusionTexture },
				{ "emissiveTexture", mat.m_emissiveTexture },
				{ "displacementTexture", mat.m_displacementTexture }
			}
		);
	}

	std::ofstream infoFile(dstFilePath, std::ios::out | std::ios::trunc);
	infoFile << std::setw(4) << j << std::endl;
	infoFile.close();
}

int main()
{
	while (true)
	{
		std::string srcFileName;
		std::string dstFileName;
		nlohmann::json j;

		std::cout << "Source File:" << std::endl;
		std::cin >> srcFileName;
		std::cout << "Destination File:" << std::endl;
		std::cin >> dstFileName;
		std::cout << "Merge objects by material? (yes = 1, no = 0)";
		bool mergeByMaterial = false;
		std::cin >> mergeByMaterial;
		std::cout << "Invert UV y-axis? (yes = 1, no = 0)";
		bool invertTexcoordY = false;
		std::cin >> invertTexcoordY;

		// load scene
		WavefrontOBJLoader objLoader;
		auto model = objLoader.loadModel(srcFileName, mergeByMaterial, invertTexcoordY);

		// create material library
		createMaterialLibrary(model.m_materials, dstFileName + ".matlib");

		j["meshFile"] = dstFileName + ".mesh";
		j["materialLibraryFile"] = dstFileName + ".matlib";
		j["subMeshes"] = nlohmann::json::array();
		j["subMeshInstances"] = nlohmann::json::array();

		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);

		size_t fileOffset = 0;
		size_t subMeshIndex = 0;

		for (const auto &mesh : model.m_meshes)
		{
			meshopt_Stream streams[] =
			{
				{mesh.m_positions.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
				{mesh.m_normals.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
				{mesh.m_texCoords.data(), sizeof(glm::vec2), sizeof(glm::vec2)},
			};

			const size_t totalIndices = mesh.m_indices.size();

			// generate indices
			std::vector<unsigned int> remap(totalIndices);
			size_t totalVertices = meshopt_generateVertexRemapMulti(remap.data(), mesh.m_indices.data(), totalIndices, mesh.m_positions.size(), streams, 3);

			// fill new index and vertex buffers
			std::vector<uint16_t> indices(totalIndices);
			std::vector<glm::vec3> positions(totalVertices);
			std::vector<glm::vec3> normals(totalVertices);
			std::vector<glm::vec2> texCoords(totalVertices);

			meshopt_remapIndexBuffer(indices.data(), mesh.m_indices.data(), totalIndices, remap.data());
			meshopt_remapVertexBuffer(positions.data(), mesh.m_positions.data(), mesh.m_positions.size(), sizeof(float) * 3, remap.data());
			meshopt_remapVertexBuffer(normals.data(), mesh.m_normals.data(), mesh.m_normals.size(), sizeof(float) * 3, remap.data());
			meshopt_remapVertexBuffer(texCoords.data(), mesh.m_texCoords.data(), mesh.m_texCoords.size(), sizeof(float) * 2, remap.data());

			// optimize vertex cache
			meshopt_optimizeVertexCache(indices.data(), indices.data(), totalIndices, totalVertices);

			// optimize vertex order
			meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), totalIndices, totalVertices);
			meshopt_remapIndexBuffer(indices.data(), indices.data(), totalIndices, remap.data());
			meshopt_remapVertexBuffer(positions.data(), positions.data(), positions.size(), sizeof(float) * 3, remap.data());
			meshopt_remapVertexBuffer(normals.data(), normals.data(), positions.size(), sizeof(float) * 3, remap.data());
			meshopt_remapVertexBuffer(texCoords.data(), texCoords.data(), positions.size(), sizeof(float) * 2, remap.data());

			// write to file
			dstFile.write((const char *)positions.data(), positions.size() * sizeof(glm::vec3));
			dstFile.write((const char *)normals.data(), normals.size() * sizeof(glm::vec3));
			dstFile.write((const char *)texCoords.data(), texCoords.size() * sizeof(glm::vec2));
			dstFile.write((const char *)indices.data(), indices.size() * sizeof(uint16_t));

			j["subMeshes"].push_back(
				{
					{ "name", mesh.m_name },
					{ "dataOffset", fileOffset },
					{ "vertexCount", positions.size() },
					{ "indexCount", indices.size() },
					{ "minCorner", { mesh.m_aabbMin.x, mesh.m_aabbMin.y, mesh.m_aabbMin.z } },
					{ "maxCorner", { mesh.m_aabbMax.x, mesh.m_aabbMax.y, mesh.m_aabbMax.z } }
				});

			j["subMeshInstances"].push_back(
				{
					{ "name", mesh.m_name },
					{ "subMesh", j["subMeshes"].size() - 1 },
					{ "material", mesh.m_materialIndex }
				});

			fileOffset += positions.size() * sizeof(glm::vec3);
			fileOffset += normals.size() * sizeof(glm::vec3);
			fileOffset += texCoords.size() * sizeof(glm::vec2);
			fileOffset += indices.size() * sizeof(uint16_t);

			std::cout << "Processed SubMesh # " << subMeshIndex++ << std::endl;
		}

		j["minCorner"] = { model.m_aabbMin.x, model.m_aabbMin.y, model.m_aabbMin.z };
		j["maxCorner"] = { model.m_aabbMax.x, model.m_aabbMax.y, model.m_aabbMax.z };
		
		dstFile.close();

		std::ofstream infoFile(dstFileName + ".info", std::ios::out | std::ios::trunc);
		infoFile << std::setw(4) << j << std::endl;
		infoFile.close();
	}
}
