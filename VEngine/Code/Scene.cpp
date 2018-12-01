#include "Scene.h"
#include "ECS/System/RenderSystem.h"
#include <fstream>
#include <json.h>
#include <memory>

void VEngine::Scene::load(RenderSystem &renderSystem, std::string filepath)
{
	nlohmann::json info;
	{
		std::ifstream file(filepath);
		file >> info;
	}

	uint32_t vertexSize = info["VertexSize"];
	uint32_t indexSize = info["IndexSize"];

	renderSystem.reserveMeshBuffer(vertexSize + indexSize);

	Mesh mesh;
	for (auto &subMeshInfo : info["SubMeshes"])
	{
		SubMesh subMesh = {};
		subMesh.m_name = subMeshInfo["Name"].get<std::string>();
		subMesh.m_vertexOffset = subMeshInfo["VertexOffset"];
		subMesh.m_vertexSize = subMeshInfo["VertexSize"];
		subMesh.m_indexOffset = subMeshInfo["IndexOffset"];
		subMesh.m_indexSize = subMeshInfo["IndexSize"];
		subMesh.m_indexCount = subMesh.m_indexSize / sizeof(uint32_t);
		subMesh.m_min = glm::vec3(subMeshInfo["Min"][0], subMeshInfo["Min"][1], subMeshInfo["Min"][2]);
		subMesh.m_max = glm::vec3(subMeshInfo["Max"][0], subMeshInfo["Max"][1], subMeshInfo["Max"][2]);
		subMesh.m_material.m_alpha = Material::Alpha(subMeshInfo["Material"]["Alpha"]);
		subMesh.m_material.m_albedoFactor = glm::vec3(subMeshInfo["Material"]["AlbedoFactor"][0], subMeshInfo["Material"]["AlbedoFactor"][1], subMeshInfo["Material"]["AlbedoFactor"][2]);
		subMesh.m_material.m_metallicFactor = subMeshInfo["Material"]["MetallicFactor"];
		subMesh.m_material.m_roughnessFactor = subMeshInfo["Material"]["RoughnessFactor"];
		subMesh.m_material.m_emissiveFactor = glm::vec3(subMeshInfo["Material"]["EmissiveFactor"][0], subMeshInfo["Material"]["EmissiveFactor"][1], subMeshInfo["Material"]["EmissiveFactor"][2]);
		
		mesh.m_subMeshes.push_back(subMesh);
	}

	m_meshes[filepath] = mesh;

	std::ifstream file(info["MeshFile"].get<std::string>(), std::ios::binary);
	std::unique_ptr<unsigned char[]> meshData = std::make_unique<unsigned char[]>(vertexSize + indexSize);

	file.read((char *)meshData.get(), vertexSize + indexSize);

	renderSystem.uploadMeshData(meshData.get(), vertexSize, meshData.get() + vertexSize, indexSize);
}

void VEngine::Scene::unload(RenderSystem &renderSystem)
{
}
