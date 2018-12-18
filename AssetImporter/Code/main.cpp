#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <exception>
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
#include <json.h>

#undef min
#undef max

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
};

void fatalExit(const char *message, int exitCode)
{
	MessageBox(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}

int main()
{
	while (true)
	{
		std::string srcFileName;
		std::string dstFileName;
		nlohmann::json j;

		j["SubMeshes"] = nlohmann::json::array();

		std::cout << "Source File:" << std::endl;
		std::cin >> srcFileName;
		std::cout << "Destination File:" << std::endl;
		std::cin >> dstFileName;

		// load scene
		Assimp::Importer importer;
		const aiScene *scene = importer.ReadFile(srcFileName, aiProcess_Triangulate
			| aiProcess_FlipUVs
			| aiProcess_JoinIdenticalVertices
			| aiProcess_GenSmoothNormals
			| aiProcess_ImproveCacheLocality
			| aiProcess_GenUVCoords
			| aiProcess_FindInvalidData
			| aiProcess_ValidateDataStructure
			| aiProcess_PreTransformVertices);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			fatalExit(importer.GetErrorString(), EXIT_FAILURE);
		}

		// assert scene has at least one mesh
		if (!scene->mNumMeshes)
		{
			fatalExit("Mesh has no submeshes!", EXIT_FAILURE);
		}

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		glm::vec3 minMeshCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());

		j["MeshFile"] = dstFileName;
		j["SubMeshCount"] = scene->mNumMeshes;

		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			std::cout << "Processing submesh #" << i << std::endl;

			aiMesh *mesh = scene->mMeshes[i];

			if (!mesh->mTextureCoords[0])
			{
				std::cout << "Mesh has no TexCoords!" << std::endl;
			}

			glm::vec3 minSubMeshCorner = glm::vec3(std::numeric_limits<float>::max());
			glm::vec3 maxSubMeshCorner = glm::vec3(std::numeric_limits<float>::lowest());
			size_t vertexStart = vertices.size() * sizeof(Vertex);
			size_t indexStart = indices.size() * sizeof(uint32_t);

			// create vertices
			for (std::uint32_t j = 0; j < mesh->mNumVertices; ++j)
			{
				Vertex vertex;

				// position
				{
					vertex.position.x = mesh->mVertices[j].x;
					vertex.position.y = mesh->mVertices[j].y;
					vertex.position.z = mesh->mVertices[j].z;
					minSubMeshCorner = glm::min(minSubMeshCorner, vertex.position);
					maxSubMeshCorner = glm::max(maxSubMeshCorner, vertex.position);
					minMeshCorner = glm::min(minMeshCorner, vertex.position);
					maxMeshCorner = glm::max(maxMeshCorner, vertex.position);
				}

				// normal
				{
					vertex.normal.x = mesh->mNormals[j].x;
					vertex.normal.y = mesh->mNormals[j].y;
					vertex.normal.z = mesh->mNormals[j].z;
					vertex.normal = glm::normalize(vertex.normal);
				}

				// texCoord
				{
					if (mesh->mTextureCoords[0])
					{
						vertex.texCoord.x = mesh->mTextureCoords[0][j].x;
						vertex.texCoord.y = mesh->mTextureCoords[0][j].y;
					}
					else
					{
						vertex.texCoord = glm::vec2(0.0f, 0.0f);
					}
				}

				vertices.push_back(vertex);
			}

			for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
			{
				aiFace &face = mesh->mFaces[j];

				for (unsigned int k = 0; k < face.mNumIndices; ++k)
				{
					indices.push_back(face.mIndices[k]);
				}
			}

			aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

			std::string albedoPath = "";
			std::string normalPath = "";
			std::string roughnessPath = "";
			std::string emissivePath = "";

			auto findAndReplace = [](std::string &data, std::string toSearch, std::string replaceStr)
			{
				// Get the first occurrence
				size_t pos = data.find(toSearch);

				// Repeat till end is reached
				while (pos != std::string::npos)
				{
					// Replace this occurrence of Sub String
					data.replace(pos, toSearch.size(), replaceStr);
					// Get the next occurrence from the current position
					pos = data.find(toSearch, pos + toSearch.size());
				}
			};

			if (material->GetTextureCount(aiTextureType_DIFFUSE))
			{
				aiString path;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
				albedoPath = path.C_Str();
				findAndReplace(albedoPath, "\\", "/");
				albedoPath = "Resources/Textures/" + albedoPath;
			}

			if (material->GetTextureCount(aiTextureType_NORMALS))
			{
				aiString path;
				material->GetTexture(aiTextureType_NORMALS, 0, &path);
				normalPath = path.C_Str();
				findAndReplace(normalPath, "\\", "/");
				normalPath = "Resources/Textures/" + normalPath;
			}

			if (material->GetTextureCount(aiTextureType_SPECULAR))
			{
				aiString path;
				material->GetTexture(aiTextureType_SPECULAR, 0, &path);
				roughnessPath = path.C_Str();
				findAndReplace(roughnessPath, "\\", "/");
				roughnessPath = "Resources/Textures/" + roughnessPath;
			}

			if (material->GetTextureCount(aiTextureType_EMISSIVE))
			{
				aiString path;
				material->GetTexture(aiTextureType_EMISSIVE, 0, &path);
				emissivePath = path.C_Str();
				findAndReplace(emissivePath, "\\", "/");
				emissivePath = "Resources/Textures/" + emissivePath;
			}

			j["SubMeshes"].push_back(
				{
					{ "Name", mesh->mName.C_Str() },
					{ "VertexOffset", vertexStart },
					{ "VertexSize", vertices.size() * sizeof(Vertex) - vertexStart },
					{ "IndexOffset", indexStart },
					{ "IndexSize", indices.size() * sizeof(uint32_t) - indexStart },
					{ "Min", { minSubMeshCorner.x, minSubMeshCorner.y, minSubMeshCorner.z } },
					{ "Max", { maxSubMeshCorner.x, maxSubMeshCorner.y, maxSubMeshCorner.z } },
					{ "Material",
						{
							{ "Alpha", 0},
							{ "AlbedoFactor", { 1.0f, 1.0f, 1.0f } },
							{ "MetallicFactor", 0.0f },
							{ "RoughnessFactor", 0.f },
							{ "EmissiveFactor", { 0.0f, 0.0f, 0.0f } },
							{ "AlbedoTexture", albedoPath },
							{ "NormalTexture", normalPath},
							{ "MetallicTexture", "" },
							{ "RoughnessTexture", roughnessPath },
							{ "OcclusionTexture", "" },
							{ "EmissiveTexture", emissivePath },
							{ "DisplacementTexture", "" }
						}
					}
				});
		}

		j["Min"] = { minMeshCorner.x, minMeshCorner.y, minMeshCorner.z };
		j["Max"] = { maxMeshCorner.x, maxMeshCorner.y, maxMeshCorner.z };
		j["VertexSize"] = vertices.size() * sizeof(Vertex);
		j["IndexSize"] = indices.size() * sizeof(uint32_t);

		std::ofstream dstFile(dstFileName, std::ios::out | std::ios::binary | std::ios::trunc);
		dstFile.write((const char *)vertices.data(), vertices.size() * sizeof(Vertex));
		dstFile.write((const char *)indices.data(), indices.size() * sizeof(uint32_t));
		dstFile.close();

		std::ofstream infoFile(dstFileName.substr(0, dstFileName.find_last_of('.')) + ".mat", std::ios::out | std::ios::trunc);
		infoFile << std::setw(4) << j << std::endl;
		infoFile.close();
	}
}