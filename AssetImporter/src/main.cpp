#include <iostream>
#include <cassert>
#include <vector>
#include <glm/mat3x3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm\vec3.hpp>
#include <glm\vec2.hpp>
#include <fstream>
#include <limits>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <unordered_set>
#include "meshoptimizer-0.12/meshoptimizer.h"
#include "mikktspace.h"
#include <glm\geometric.hpp>

#include "Utility.h"
#include "WavefrontOBJLoader.h"
#include "BinaryMeshLoader.h"

// Returns the number of faces (triangles/quads) on the mesh to be processed.
int mikktGetNumFaces(const SMikkTSpaceContext *pContext)
{
	Mesh *mesh = reinterpret_cast<Mesh *>(pContext->m_pUserData);
	return mesh->m_positions.size() / 3;
}

// Returns the number of vertices on face number iFace
// iFace is a number in the range {0, 1, ..., getNumFaces()-1}
int mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace)
{
	return 3;
}

// returns the position/normal/texcoord of the referenced face of vertex number iVert.
// iVert is in the range {0,1,2} for triangles and {0,1,2,3} for quads.
void mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert)
{
	Mesh *mesh = reinterpret_cast<Mesh *>(pContext->m_pUserData);
	const auto &pos = mesh->m_positions[iFace * 3 + iVert];
	fvPosOut[0] = pos.x;
	fvPosOut[1] = pos.y;
	fvPosOut[2] = pos.z;
}

void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert)
{
	Mesh *mesh = reinterpret_cast<Mesh *>(pContext->m_pUserData);
	const auto &norm = mesh->m_normals[iFace * 3 + iVert];
	fvNormOut[0] = norm.x;
	fvNormOut[1] = norm.y;
	fvNormOut[2] = norm.z;
}

void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert)
{
	Mesh *mesh = reinterpret_cast<Mesh *>(pContext->m_pUserData);
	const auto &tc = mesh->m_texCoords[iFace * 3 + iVert];
	fvTexcOut[0] = tc.x;
	fvTexcOut[1] = tc.y;
}

// either (or both) of the two setTSpace callbacks can be set.
// The call-back m_setTSpaceBasic() is sufficient for basic normal mapping.

// This function is used to return the tangent and fSign to the application.
// fvTangent is a unit length vector.
// For normal maps it is sufficient to use the following simplified version of the bitangent which is generated at pixel/vertex level.
// bitangent = fSign * cross(vN, tangent);
// Note that the results are returned unindexed. It is possible to generate a new index list
// But averaging/overwriting tangent spaces by using an already existing index list WILL produce INCRORRECT results.
// DO NOT! use an already existing index list.
void mikktSetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
{
	Mesh *mesh = reinterpret_cast<Mesh *>(pContext->m_pUserData);
	mesh->m_tangents[iFace * 3 + iVert] = glm::vec4(fvTangent[0], fvTangent[1], fvTangent[2], fSign);
}

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
				{ "alphaMode", (int)mat.m_alpha },
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

namespace
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
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

template<typename T>
struct IndexedMesh
{
	std::vector<T> indices;
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec4> tangents;
	std::vector<glm::vec2> texCoords;
};

template<typename T>
IndexedMesh<T> generateOptimizedMesh(size_t faceCount, const glm::vec3 *positions, const glm::vec3 *normals, const glm::vec4 *tangents, const glm::vec2 *texCoords, bool optimizeVertexOrder)
{
	meshopt_Stream streams[] =
	{
		{positions, sizeof(glm::vec3), sizeof(glm::vec3)},
		{normals, sizeof(glm::vec3), sizeof(glm::vec3)},
		{tangents, sizeof(glm::vec4), sizeof(glm::vec4)},
		{texCoords, sizeof(glm::vec2), sizeof(glm::vec2)},
	};

	// generate indices
	std::vector<unsigned int> remap(faceCount * 3);
	size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.data(), nullptr, faceCount * 3, faceCount * 3, streams, 4);

	// fill new index and vertex buffers
	std::vector<T> indices(faceCount * 3);
	std::vector<glm::vec3> positionsIndexed(uniqueVertexCount);
	std::vector<glm::vec3> normalsIndexed(uniqueVertexCount);
	std::vector<glm::vec4> tangentsIndexed(uniqueVertexCount);
	std::vector<glm::vec2> texCoordsIndexed(uniqueVertexCount);

	meshopt_remapIndexBuffer(indices.data(), (T *)nullptr, indices.size(), remap.data());
	meshopt_remapVertexBuffer(positionsIndexed.data(), positions, faceCount * 3, sizeof(glm::vec3), remap.data());
	meshopt_remapVertexBuffer(normalsIndexed.data(), normals, faceCount * 3, sizeof(glm::vec3), remap.data());
	meshopt_remapVertexBuffer(tangentsIndexed.data(), tangents, faceCount * 3, sizeof(glm::vec4), remap.data());
	meshopt_remapVertexBuffer(texCoordsIndexed.data(), texCoords, faceCount * 3, sizeof(glm::vec2), remap.data());

	// optimize vertex cache
	meshopt_optimizeVertexCache(indices.data(), indices.data(), faceCount * 3, uniqueVertexCount);


	// optimize vertex order
	if (optimizeVertexOrder)
	{
		meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), uniqueVertexCount);
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
		meshopt_remapVertexBuffer(positionsIndexed.data(), positionsIndexed.data(), positionsIndexed.size(), sizeof(glm::vec3), remap.data());
		meshopt_remapVertexBuffer(normalsIndexed.data(), normalsIndexed.data(), normalsIndexed.size(), sizeof(glm::vec3), remap.data());
		meshopt_remapVertexBuffer(tangentsIndexed.data(), tangentsIndexed.data(), tangentsIndexed.size(), sizeof(glm::vec4), remap.data());
		meshopt_remapVertexBuffer(texCoordsIndexed.data(), texCoordsIndexed.data(), texCoordsIndexed.size(), sizeof(glm::vec2), remap.data());
	}

	return { indices, positionsIndexed, normalsIndexed, tangentsIndexed, texCoordsIndexed };
}

int main()
{
	SMikkTSpaceInterface mikkTSpaceInterface = {};
	mikkTSpaceInterface.m_getNumFaces = mikktGetNumFaces;
	mikkTSpaceInterface.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
	mikkTSpaceInterface.m_getPosition = mikktGetPosition;
	mikkTSpaceInterface.m_getNormal = mikktGetNormal;
	mikkTSpaceInterface.m_getTexCoord = mikktGetTexCoord;
	mikkTSpaceInterface.m_setTSpaceBasic = mikktSetTSpaceBasic;
	mikkTSpaceInterface.m_setTSpace = nullptr;


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
		//WavefrontOBJLoader objLoader;
		//auto model = objLoader.loadModel(srcFileName, mergeByMaterial, invertTexcoordY);
		BinaryMeshLoader bmLoader;
		auto model = bmLoader.loadModel(srcFileName, mergeByMaterial, invertTexcoordY);


		// create material library
		createMaterialLibrary(model.m_materials, dstFileName + ".matlib");

		j["meshFile"] = dstFileName + ".mesh";
		j["materialLibraryFile"] = dstFileName + ".matlib";
		j["subMeshes"] = nlohmann::json::array();
		j["subMeshInstances"] = nlohmann::json::array();

		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);

		size_t fileOffset = 0;
		size_t subMeshIndex = 0;

		for (auto &mesh : model.m_meshes)
		{
			// generate tangents
			{
				SMikkTSpaceContext mikkTSpaceContext = {};
				mikkTSpaceContext.m_pInterface = &mikkTSpaceInterface;
				mikkTSpaceContext.m_pUserData = &mesh;

				auto result = genTangSpaceDefault(&mikkTSpaceContext);
				assert(result != 0);
			}

			// generate optimized 32bit indexed mesh
			auto indexedMesh32 = generateOptimizedMesh<uint32_t>(mesh.m_positions.size() / 3, mesh.m_positions.data(), mesh.m_normals.data(), mesh.m_tangents.data(), mesh.m_texCoords.data(), false);

			// generate smaller 64k meshes
			{
				glm::vec3 aabbMin = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
				glm::vec2 uvAabbMin = glm::vec2(std::numeric_limits<float>::max());
				glm::vec2 uvAabbMax = glm::vec2(std::numeric_limits<float>::lowest());

				std::vector<glm::vec3> positions;
				std::vector<glm::vec3> normals;
				std::vector<glm::vec4> tangents;
				std::vector<glm::vec2> texCoords;

				// keep track of the number of unique vertices -> we are targeting 16bit indices, so we need to stay below 2^16 - 1 vertices
				std::unordered_set<Vertex, VertexHash> vertexSet;

				for (size_t i = 0; i < indexedMesh32.indices.size(); i += 3)
				{
					// process face
					for (size_t j = 0; j < 3; ++j)
					{
						uint32_t index = indexedMesh32.indices[i + j];

						Vertex v;
						v.position = indexedMesh32.positions[index];
						v.normal = indexedMesh32.normals[index];
						v.tangent = indexedMesh32.tangents[index];
						v.texCoord = indexedMesh32.texCoords[index];

						vertexSet.insert(v);

						positions.push_back(v.position);
						normals.push_back(v.normal);
						tangents.push_back(v.tangent);
						texCoords.push_back(v.texCoord);

						aabbMin = glm::min(aabbMin, v.position);
						aabbMax = glm::max(aabbMax, v.position);

						uvAabbMin = glm::min(uvAabbMin, v.texCoord);
						uvAabbMax = glm::max(uvAabbMax, v.texCoord);
					}

					// we reached 64k faces or 64k unique vertices or we processed the whole mesh
					if ((i / 3 + 1) > UINT16_MAX || (vertexSet.size() + 3) > UINT16_MAX || (i + 3) == indexedMesh32.indices.size())
					{
						auto indexedMesh16 = generateOptimizedMesh<uint16_t>(positions.size() / 3, positions.data(), normals.data(), tangents.data(), texCoords.data(), true);

						// quantize data
						std::vector<uint16_t> quantizedPositions;
						std::vector<uint16_t> quantizedQTangents;
						std::vector<uint16_t> quantizedTexCoords;
						quantizedPositions.reserve(3 * indexedMesh16.positions.size());
						quantizedQTangents.reserve(4 * indexedMesh16.positions.size());
						quantizedTexCoords.reserve(2 * indexedMesh16.positions.size());

						glm::vec3 scale = 1.0f / (aabbMax - aabbMin);
						glm::vec3 bias = -aabbMin * scale;

						glm::vec2 texCoordScale = 1.0f / (uvAabbMax - uvAabbMin);
						glm::vec2 texCoordBias = -uvAabbMin * texCoordScale;

						for (size_t j = 0; j < indexedMesh16.positions.size(); ++j)
						{
							// position
							{
								glm::vec3 normalizedPosition = indexedMesh16.positions[j] * scale + bias;
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.x, 16));
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.y, 16));
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.z, 16));
							}

							// texcoord
							{
								glm::vec2 normalizedTexCoord = indexedMesh16.texCoords[j] * texCoordScale + texCoordBias;
								quantizedTexCoords.push_back(meshopt_quantizeUnorm(normalizedTexCoord.x, 16));
								quantizedTexCoords.push_back(meshopt_quantizeUnorm(normalizedTexCoord.y, 16));
							}

							// qtangent
							{
								glm::vec4 tangent = indexedMesh16.tangents[j];
								glm::vec3 normal = indexedMesh16.normals[j];
								glm::vec3 bitangent = glm::cross(normal, glm::vec3(tangent));

								glm::mat3 tbn = glm::mat3(glm::normalize(glm::vec3(tangent)),
									glm::normalize(bitangent),
									glm::normalize(normal));
								glm::quat q = glm::quat_cast(tbn);

								// ensure q.w is always positive
								if (q.w < 0.0f)
								{
									q = -q;
								}

								// make sure q.w never becomes 0.0
								constexpr float bias = 1.0f / INT16_MAX;
								if (q.w < bias)
								{
									float normFactor = sqrtf(1.0f - bias * bias);
									q.x *= normFactor;
									q.y *= normFactor;
									q.z *= normFactor;
									q.w = bias;
								}

								// encode sign of bitangent in quaternion (q == -q)
								if (tangent.w < 0.0f)
								{
									q = -q;
								}

								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.x * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.y * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.z * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.w * 0.5f + 0.5f, 16));
							}
						}


						// write to file
						dstFile.write((const char *)quantizedPositions.data(), quantizedPositions.size() * sizeof(uint16_t));
						dstFile.write((const char *)quantizedQTangents.data(), quantizedQTangents.size() * sizeof(uint16_t));
						dstFile.write((const char *)quantizedTexCoords.data(), quantizedTexCoords.size() * sizeof(uint16_t));
						dstFile.write((const char *)indexedMesh16.indices.data(), indexedMesh16.indices.size() * sizeof(uint16_t));

						j["subMeshes"].push_back(
							{
								{ "name", mesh.m_name },
								{ "dataOffset", fileOffset },
								{ "vertexCount", indexedMesh16.positions.size() },
								{ "indexCount", indexedMesh16.indices.size() },
								{ "minCorner", { aabbMin.x,  aabbMin.y,  aabbMin.z } },
								{ "maxCorner", { aabbMax.x,  aabbMax.y,  aabbMax.z } },
								{ "texCoordMin", { uvAabbMin.x,  uvAabbMin.y } },
								{ "texCoordMax", { uvAabbMax.x,  uvAabbMax.y } }
							});

						j["subMeshInstances"].push_back(
							{
								{ "name", mesh.m_name },
								{ "subMesh", j["subMeshes"].size() - 1 },
								{ "material", mesh.m_materialIndex }
							});

						fileOffset += quantizedPositions.size() * sizeof(uint16_t);
						fileOffset += quantizedQTangents.size() * sizeof(uint16_t);
						fileOffset += quantizedTexCoords.size() * sizeof(uint16_t);
						fileOffset += indexedMesh16.indices.size() * sizeof(uint16_t);

						aabbMin = glm::vec3(std::numeric_limits<float>::max());
						aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
						uvAabbMin = glm::vec2(std::numeric_limits<float>::max());
						uvAabbMax = glm::vec2(std::numeric_limits<float>::lowest());
						positions.clear();
						normals.clear();
						tangents.clear();
						texCoords.clear();
						vertexSet.clear();

						std::cout << "Processed SubMesh # " << subMeshIndex++ << std::endl;
					}
				}
			}
		}

		j["minCorner"] = { model.m_aabbMin.x, model.m_aabbMin.y, model.m_aabbMin.z };
		j["maxCorner"] = { model.m_aabbMax.x, model.m_aabbMax.y, model.m_aabbMax.z };

		dstFile.close();

		std::ofstream infoFile(dstFileName + ".info", std::ios::out | std::ios::trunc);
		infoFile << std::setw(4) << j << std::endl;
		infoFile.close();
	}
}
