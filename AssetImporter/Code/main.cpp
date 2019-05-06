#include <iostream>
#include <cassert>
#include <vector>
#include <glm\vec3.hpp>
#include <glm\vec2.hpp>
#include <glm\geometric.hpp>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <unordered_map>

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

static bool invertHandedness = true;

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

		// load scene
		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		std::string warn;
		std::string err;

		bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, srcFileName.c_str(), nullptr, true);

		if (!warn.empty()) {
			std::cout << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << err << std::endl;
		}

		if (!ret)
		{
			fatalExit("Failed to load file!", EXIT_FAILURE);
		}

		// create material library
		createMaterialLibrary(objMaterials, dstFileName + ".matlib");

		j["meshFile"] = dstFileName + ".mesh";
		j["materialLibraryFile"] = dstFileName + ".matlib";
		j["subMeshes"] = nlohmann::json::array();
		j["subMeshInstances"] = nlohmann::json::array();

		glm::vec3 minMeshCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());

		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);

		size_t fileOffset = 0;
		size_t subMeshIndex = 0;
		for (const auto &shape : objShapes)
		{
			// sort faces by materials to split up submeshes if multiple materials are used on a single submesh
			std::map<size_t, std::vector<tinyobj::index_t>> materialToIndex;

			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3)
			{
				auto &vec = materialToIndex[shape.mesh.material_ids[i / 3]];
				vec.push_back(shape.mesh.indices[i]);
				vec.push_back(shape.mesh.indices[i + 1]);
				vec.push_back(shape.mesh.indices[i + 2]);
			}

			// iterate over all index lists of this  submesh.
			// in most cases this will only be one list except if per-face materials are present in the mesh
			for (const auto &materialIndicesPair : materialToIndex)
			{
				std::cout << "Processing submesh #" << subMeshIndex++ << std::endl;

				glm::vec3 minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());

				std::vector<glm::vec3> positions;
				std::vector<glm::vec3> normals;
				std::vector<glm::vec2> texCoords;
				std::vector<uint32_t> indices;

				std::unordered_map<Vertex, uint32_t, VertexHash> vertexToIndex;

				// iterate over index list and build index/vertex data for this submesh
				for (const auto &index : materialIndicesPair.second)
				{
					Vertex vertex;

					vertex.position.x = objAttrib.vertices[index.vertex_index * 3 + 0];
					vertex.position.y = objAttrib.vertices[index.vertex_index * 3 + 1];
					vertex.position.z = objAttrib.vertices[index.vertex_index * 3 + 2];

					vertex.normal.x = objAttrib.normals[index.normal_index * 3 + 0];
					vertex.normal.y = objAttrib.normals[index.normal_index * 3 + 1];
					vertex.normal.z = objAttrib.normals[index.normal_index * 3 + 2];

					if (objAttrib.texcoords.size() > index.texcoord_index * 2 + 1)
					{
						vertex.texCoord.x = objAttrib.texcoords[index.texcoord_index * 2 + 0];
						vertex.texCoord.y = objAttrib.texcoords[index.texcoord_index * 2 + 1];
						if (invertHandedness)
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

					// if we havent encountered this vertex before, add it to the map
					if (vertexToIndex.count(vertex) == 0)
					{
						vertexToIndex[vertex] = static_cast<uint32_t>(positions.size());

						positions.push_back(vertex.position);
						normals.push_back(vertex.normal);
						texCoords.push_back(vertex.texCoord);
					}
					indices.push_back(vertexToIndex[vertex]);
				}
				
				dstFile.write((const char *)positions.data(), positions.size() * sizeof(glm::vec3));
				dstFile.write((const char *)normals.data(), normals.size() * sizeof(glm::vec3));
				dstFile.write((const char *)texCoords.data(), texCoords.size() * sizeof(glm::vec2));
				dstFile.write((const char *)indices.data(), indices.size() * sizeof(uint32_t));

				j["subMeshes"].push_back(
					{
						{ "name", shape.name },
						{ "dataOffset", fileOffset },
						{ "vertexCount", positions.size() },
						{ "indexCount", indices.size() },
						{ "minCorner", { minSubMeshCorner.x, minSubMeshCorner.y, minSubMeshCorner.z } },
						{ "maxCorner", { maxSubMeshCorner.x, maxSubMeshCorner.y, maxSubMeshCorner.z } }
					});

				j["subMeshInstances"].push_back(
					{
						{ "name", shape.name },
						{ "subMesh", j["subMeshes"].size() - 1 },
						{ "material", materialIndicesPair.first }
					});

				fileOffset += positions.size() * sizeof(glm::vec3);
				fileOffset += normals.size() * sizeof(glm::vec3);
				fileOffset += texCoords.size() * sizeof(glm::vec2);
				fileOffset += indices.size() * sizeof(uint32_t);
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
