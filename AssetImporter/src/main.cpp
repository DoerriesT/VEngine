#include <iostream>
#include <cassert>
#include <vector>
#include <glm\vec3.hpp>
#include <glm\vec2.hpp>
#include <glm\geometric.hpp>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <unordered_map>
#include <set>
#include "meshoptimizer-0.12/meshoptimizer.h"
#include <Windows.h>

#undef min
#undef max
#undef OPAQUE

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
};

inline bool operator==(const Vertex &lhs, const Vertex &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

template <class T>
inline void hashCombine(size_t &s, const T &v)
{
	std::hash<T> h;
	s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

struct VertexHash
{
	inline size_t operator()(const Vertex &value) const
	{
		size_t hashValue = 0;

		for (size_t i = 0; i < sizeof(Vertex); ++i)
		{
			hashCombine(hashValue, reinterpret_cast<const char *>(&value)[i]);
		}

		return hashValue;
	}
};

struct Material
{
	enum class Alpha
	{
		OPAQUE, MASKED, BLENDED
	};

	Alpha m_alpha;
	glm::vec3 m_albedoFactor;
	float m_metalnessFactor;
	float m_roughnessFactor;
	glm::vec3 m_emissiveFactor;
	float m_opacity;
	std::string m_albedoTexture;
	std::string m_normalTexture;
	std::string m_metalnessTexture;
	std::string m_roughnessTexture;
	std::string m_occlusionTexture;
	std::string m_emissiveTexture;
	std::string m_displacementTexture;
};

void fatalExit(const char *message, int exitCode)
{
	MessageBox(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}

void createMaterialLibrary(const std::vector<tinyobj::material_t> &objMaterials, const std::string dstFilePath)
{
	nlohmann::json j;

	j["count"] = objMaterials.size();
	j["materials"] = nlohmann::json::array();

	for (const auto &objMaterial : objMaterials)
	{
		j["materials"].push_back(
			{
				{ "name", objMaterial.name },
				{ "alphaMode", 0 },
				{ "albedo", { objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2] } },
				{ "metalness", objMaterial.metallic },
				{ "roughness", objMaterial.roughness },
				{ "emissive", { objMaterial.emission[0], objMaterial.emission[1], objMaterial.emission[2] } },
				{ "opacity", 1.0f },
				{ "albedoTexture", objMaterial.diffuse_texname },
				{ "normalTexture", objMaterial.normal_texname.empty() ? objMaterial.bump_texname : objMaterial.normal_texname },
				{ "metalnessTexture", objMaterial.metallic_texname },
				{ "roughnessTexture", objMaterial.roughness_texname.empty() ? objMaterial.specular_texname : objMaterial.roughness_texname },
				{ "occlusionTexture", "" },
				{ "emissiveTexture", objMaterial.emissive_texname },
				{ "displacementTexture", objMaterial.displacement_texname }
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
		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		std::string warn;
		std::string err;

		bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, srcFileName.c_str(), nullptr, true);

		if (!warn.empty()) 
		{
			std::cout << warn << std::endl;
		}

		if (!err.empty()) 
		{
			std::cerr << err << std::endl;
		}

		if (!ret)
		{
			fatalExit("Failed to load file!", EXIT_FAILURE);
		}

		// create material library
		createMaterialLibrary(objMaterials, dstFileName + ".matlib");

		struct Index
		{
			uint32_t m_shapeIndex;
			uint32_t m_materialIndex;
			tinyobj::index_t m_vertexIndices[3];
		};

		std::vector<Index> unifiedIndices;

		for (size_t shapeIndex = 0; shapeIndex < objShapes.size(); ++shapeIndex)
		{
			const auto &shape = objShapes[shapeIndex];
			for (size_t index = 0; index < shape.mesh.indices.size(); index += 3)
			{
				Index unifiedIndex;
				unifiedIndex.m_shapeIndex = shapeIndex;
				unifiedIndex.m_materialIndex = shape.mesh.material_ids[index / 3];
				unifiedIndex.m_vertexIndices[0] = shape.mesh.indices[index + 0];
				unifiedIndex.m_vertexIndices[1] = shape.mesh.indices[index + 1];
				unifiedIndex.m_vertexIndices[2] = shape.mesh.indices[index + 2];

				unifiedIndices.push_back(unifiedIndex);
			}
		}

		if (mergeByMaterial)
		{
			// sort all by material
			std::stable_sort(unifiedIndices.begin(), unifiedIndices.end(), [](const auto &lhs, const auto &rhs) { return lhs.m_materialIndex < rhs.m_materialIndex; });
		}
		else
		{
			// sort only inside same shape by material
			std::stable_sort(unifiedIndices.begin(), unifiedIndices.end(),
				[](const auto &lhs, const auto &rhs) 
			{ 
				return lhs.m_shapeIndex < rhs.m_shapeIndex || (lhs.m_shapeIndex == rhs.m_shapeIndex && lhs.m_materialIndex < rhs.m_materialIndex);
			});
		}

		j["meshFile"] = dstFileName + ".mesh";
		j["materialLibraryFile"] = dstFileName + ".matlib";
		j["subMeshes"] = nlohmann::json::array();
		j["subMeshInstances"] = nlohmann::json::array();

		glm::vec3 minMeshCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());

		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);

		size_t fileOffset = 0;
		size_t subMeshIndex = 0;

		glm::vec3 minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());
		std::vector<glm::vec3> positionsRaw;
		std::vector<glm::vec3> normalsRaw;
		std::vector<glm::vec2> texCoordsRaw;
		std::set<uint32_t> shapeIndices;

		for (size_t i = 0; i < unifiedIndices.size(); ++i)
		{
			const auto &unifiedIdx = unifiedIndices[i];
			// loop over face (triangle)
			for (size_t j = 0; j < 3; ++j)
			{
				Vertex vertex;
				const auto &vertexIndex = unifiedIdx.m_vertexIndices[j];

				vertex.position.x = objAttrib.vertices[vertexIndex.vertex_index * 3 + 0];
				vertex.position.y = objAttrib.vertices[vertexIndex.vertex_index * 3 + 1];
				vertex.position.z = objAttrib.vertices[vertexIndex.vertex_index * 3 + 2];

				vertex.normal.x = objAttrib.normals[vertexIndex.normal_index * 3 + 0];
				vertex.normal.y = objAttrib.normals[vertexIndex.normal_index * 3 + 1];
				vertex.normal.z = objAttrib.normals[vertexIndex.normal_index * 3 + 2];

				if (objAttrib.texcoords.size() > vertexIndex.texcoord_index * 2 + 1)
				{
					vertex.texCoord.x = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 0];
					vertex.texCoord.y = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 1];

					if (invertTexcoordY)
					{
						vertex.texCoord.y = 1.0f - vertex.texCoord.y;
					}
				}
				else
				{
					vertex.texCoord.x = 0.0f;
					vertex.texCoord.y = 0.0f;
				}

				// find min/max corners
				minSubMeshCorner = glm::min(minSubMeshCorner, vertex.position);
				maxSubMeshCorner = glm::max(maxSubMeshCorner, vertex.position);
				minMeshCorner = glm::min(minMeshCorner, vertex.position);
				maxMeshCorner = glm::max(maxMeshCorner, vertex.position);

				positionsRaw.push_back(vertex.position);
				normalsRaw.push_back(vertex.normal);
				texCoordsRaw.push_back(vertex.texCoord);

				shapeIndices.insert(unifiedIdx.m_shapeIndex);
			}

			if (i + 1 == unifiedIndices.size()														// last index -> we're done
				|| positionsRaw.size() >= std::numeric_limits<uint16_t>::max() - 3
				|| unifiedIndices[i + 1].m_materialIndex != unifiedIdx.m_materialIndex					// next material is different -> write to file
				|| (!mergeByMaterial && unifiedIndices[i + 1].m_shapeIndex != unifiedIdx.m_shapeIndex))	// dont merge by material (keep distinct objects) and next shape is different -> write to file
			{
				meshopt_Stream streams[] = 
				{
					{positionsRaw.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
					{normalsRaw.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
					{texCoordsRaw.data(), sizeof(glm::vec2), sizeof(glm::vec2)},
				};

				const size_t totalIndices = positionsRaw.size();

				// generate indices
				std::vector<unsigned int> remap(totalIndices);
				size_t totalVertices = meshopt_generateVertexRemapMulti(remap.data(), (uint16_t*)nullptr, totalIndices, totalIndices, streams, 3);
			
				// fill new index and vertex buffers
				std::vector<uint16_t> indices(totalIndices);
				std::vector<glm::vec3> positions(totalVertices);
				std::vector<glm::vec3> normals(totalVertices);
				std::vector<glm::vec2> texCoords(totalVertices);

				meshopt_remapIndexBuffer(indices.data(), (uint16_t*)nullptr, totalIndices, remap.data());
				meshopt_remapVertexBuffer(positions.data(), positionsRaw.data(), positionsRaw.size(), sizeof(float) * 3, remap.data());
				meshopt_remapVertexBuffer(normals.data(), normalsRaw.data(), positionsRaw.size(), sizeof(float) * 3, remap.data());
				meshopt_remapVertexBuffer(texCoords.data(), texCoordsRaw.data(), positionsRaw.size(), sizeof(float) * 2, remap.data());
			
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

				std::string shapeName = "";

				for (const auto shapeIndex : shapeIndices)
				{
					shapeName += " " + objShapes[shapeIndex].name;
				}

				j["subMeshes"].push_back(
					{
						{ "name", shapeName },
						{ "dataOffset", fileOffset },
						{ "vertexCount", positions.size() },
						{ "indexCount", indices.size() },
						{ "minCorner", { minSubMeshCorner.x, minSubMeshCorner.y, minSubMeshCorner.z } },
						{ "maxCorner", { maxSubMeshCorner.x, maxSubMeshCorner.y, maxSubMeshCorner.z } }
					});

				j["subMeshInstances"].push_back(
					{
						{ "name", shapeName },
						{ "subMesh", j["subMeshes"].size() - 1 },
						{ "material", unifiedIdx.m_materialIndex }
					});

				fileOffset += positions.size() * sizeof(glm::vec3);
				fileOffset += normals.size() * sizeof(glm::vec3);
				fileOffset += texCoords.size() * sizeof(glm::vec2);
				fileOffset += indices.size() * sizeof(uint16_t);

				// reset data
				minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
				maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());
				positionsRaw.clear();
				normalsRaw.clear();
				texCoordsRaw.clear();
				shapeIndices.clear();

				std::cout << "Processed SubMesh # " << subMeshIndex++ << std::endl;
			}
		}

		j["minCorner"] = { minMeshCorner.x, minMeshCorner.y, minMeshCorner.z };
		j["maxCorner"] = { maxMeshCorner.x, maxMeshCorner.y, maxMeshCorner.z };
		
		dstFile.close();

		std::ofstream infoFile(dstFileName + ".info", std::ios::out | std::ios::trunc);
		infoFile << std::setw(4) << j << std::endl;
		infoFile.close();
	}
}
