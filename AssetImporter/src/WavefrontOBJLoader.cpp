#include "WavefrontOBJLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "Utility.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <glm\geometric.hpp>

namespace
{
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
}

Model WavefrontOBJLoader::loadModel(const std::string &filepath, bool mergeByMaterial, bool invertTexcoordY) const
{
	Model resultModel;

	// load scene
	tinyobj::attrib_t objAttrib;
	std::vector<tinyobj::shape_t> objShapes;
	std::vector<tinyobj::material_t> objMaterials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, filepath.c_str(), nullptr, true);

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
		Utility::fatalExit("Failed to load file!", EXIT_FAILURE);
	}

	// create materials
	{
		for (const auto &objMaterial : objMaterials)
		{
			Material mat{};
			mat.m_name = objMaterial.name;
			mat.m_alpha = Material::Alpha::OPAQUE;
			mat.m_albedoFactor = { objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2] };
			mat.m_metalnessFactor = objMaterial.metallic;
			mat.m_roughnessFactor = objMaterial.roughness;
			mat.m_emissiveFactor = { objMaterial.emission[0], objMaterial.emission[1], objMaterial.emission[2] };
			mat.m_opacity = 1.0f;
			mat.m_albedoTexture = objMaterial.diffuse_texname;
			mat.m_normalTexture = objMaterial.normal_texname.empty() ? objMaterial.bump_texname : objMaterial.normal_texname;
			mat.m_metalnessTexture = objMaterial.metallic_texname;
			mat.m_roughnessTexture = objMaterial.roughness_texname;
			mat.m_occlusionTexture = "";
			mat.m_emissiveTexture = objMaterial.emissive_texname;
			mat.m_displacementTexture = objMaterial.displacement_texname;

			resultModel.m_materials.push_back(mat);
		}
	}

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
			unifiedIndex.m_shapeIndex = static_cast<uint32_t>(shapeIndex);
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

	resultModel.m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	resultModel.m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());

	std::set<uint32_t> shapeIndices;
	std::unordered_map<Vertex, uint16_t, VertexHash> m_vertexToIndexMap;

	Mesh mesh = {};

	// loop over all primitives and create individual meshes
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

			mesh.m_aabbMin = glm::min(mesh.m_aabbMin, vertex.position);
			mesh.m_aabbMax = glm::max(mesh.m_aabbMax, vertex.position);

			auto it = m_vertexToIndexMap.find(vertex);
			// vertex is new
			if (it == m_vertexToIndexMap.end())
			{
				m_vertexToIndexMap[vertex] = static_cast<uint16_t>(mesh.m_positions.size());
				mesh.m_indices.push_back(static_cast<uint16_t>(mesh.m_positions.size()));

				mesh.m_positions.push_back(vertex.position);
				mesh.m_normals.push_back(vertex.normal);
				mesh.m_texCoords.push_back(vertex.texCoord);

				
			}
			// vertex is already in map
			else
			{
				mesh.m_indices.push_back(it->second);
			}

			shapeIndices.insert(unifiedIdx.m_shapeIndex);
		}

		if (i + 1 == unifiedIndices.size()														// last index -> we're done
			|| mesh.m_indices.size() >= std::numeric_limits<uint16_t>::max() - 3
			|| unifiedIndices[i + 1].m_materialIndex != unifiedIdx.m_materialIndex					// next material is different -> create new mesh
			|| (!mergeByMaterial && unifiedIndices[i + 1].m_shapeIndex != unifiedIdx.m_shapeIndex))	// dont merge by material (keep distinct objects) and next shape is different -> create new mesh
		{
			for (const auto shapeIndex : shapeIndices)
			{
				if (!mesh.m_name.empty())
				{
					mesh.m_name += "_";
				}
				mesh.m_name += objShapes[shapeIndex].name;
			}

			mesh.m_materialIndex = unifiedIdx.m_materialIndex;

			resultModel.m_aabbMin = glm::min(mesh.m_aabbMin, resultModel.m_aabbMin);
			resultModel.m_aabbMax = glm::max(mesh.m_aabbMax, resultModel.m_aabbMax);

			resultModel.m_meshes.push_back(std::move(mesh));

			// reset temp data
			mesh = {};
			shapeIndices.clear();
		}
	}

	return resultModel;
}
