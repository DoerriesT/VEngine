#include "Model.h"
#include "Utility/Utility.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include "Vulkan/VKContext.h"
#include "Vulkan/VKUtility.h"

extern VEngine::VKContext g_context;

VEngine::Model::Model(const char *filepath)
{
	// load scene
	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
		assert(false);
	}

	// assert scene has at least one mesh
	assert(scene->mNumMeshes);

	// get first mesh
	aiMesh *mesh = scene->mMeshes[0];

	// prepare vertex and index lists
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
	{
		Vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.position = vector;

		vector.x = mesh->mNormals[i].x;
		vector.y = mesh->mNormals[i].y;
		vector.z = mesh->mNormals[i].z;
		vertex.normal = glm::normalize(vector);

		if (mesh->mTextureCoords[0])
		{
			glm::vec2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.texCoord = vec;
		}
		else
		{
			vertex.texCoord = glm::vec2(0.0f, 0.0f);
		}

		vertices.push_back(vertex);
	}

	for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];

		for (uint32_t j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// upload to vulkan
	{
		// index buffer
		{
			VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

			VKBufferData stagingBuffer;
			VKUtility::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.m_buffer, stagingBuffer.m_memory);

			void *data;
			vkMapMemory(g_context.m_device, stagingBuffer.m_memory, 0, bufferSize, 0, &data);
			memcpy(data, indices.data(), (size_t)bufferSize);
			vkUnmapMemory(g_context.m_device, stagingBuffer.m_memory);

			VKUtility::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer.m_buffer, m_indexBuffer.m_memory);

			VKUtility::copyBuffer(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, stagingBuffer, m_indexBuffer, bufferSize);

			vkDestroyBuffer(g_context.m_device, stagingBuffer.m_buffer, nullptr);
			vkFreeMemory(g_context.m_device, stagingBuffer.m_memory, nullptr);
		}

		// vertex buffer
		{
			VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

			VKBufferData stagingBuffer;
			VKUtility::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.m_buffer, stagingBuffer.m_memory);

			void *data;
			vkMapMemory(g_context.m_device, stagingBuffer.m_memory, 0, bufferSize, 0, &data);
			memcpy(data, vertices.data(), (size_t)bufferSize);
			vkUnmapMemory(g_context.m_device, stagingBuffer.m_memory);

			VKUtility::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer.m_buffer, m_vertexBuffer.m_memory);

			VKUtility::copyBuffer(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, stagingBuffer, m_vertexBuffer, bufferSize);

			vkDestroyBuffer(g_context.m_device, stagingBuffer.m_buffer, nullptr);
			vkFreeMemory(g_context.m_device, stagingBuffer.m_memory, nullptr);
		}
	}

	indexCount = static_cast<uint32_t>(indices.size());
}

VEngine::VKBufferData VEngine::Model::getIndexBufferData() const
{
	return m_indexBuffer;
}

VEngine::VKBufferData VEngine::Model::getVertexBufferData() const
{
	return m_vertexBuffer;
}

uint32_t VEngine::Model::getIndexCount() const
{
	return indexCount;
}
