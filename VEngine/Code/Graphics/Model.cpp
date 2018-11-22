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
		VkDeviceSize bufferSizes[] = { sizeof(indices[0]) * indices.size(), sizeof(vertices[0]) * vertices.size() };
		VkBufferUsageFlags usages[] = { VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
		VKBufferData *buffers[] = { &m_indexBuffer, &m_vertexBuffer };

		for (size_t i = 0; i < 2; ++i)
		{
			VkDeviceSize bufferSize = bufferSizes[i];

			VKBufferData stagingBuffer;
			{
				VkBufferCreateInfo bufferInfo = {};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = bufferSize;
				bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VmaAllocationCreateInfo allocInfo = {};
				allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

				if (vmaCreateBuffer(g_context.m_allocator, &bufferInfo, &allocInfo, &stagingBuffer.m_buffer, &stagingBuffer.m_allocation, &stagingBuffer.m_info) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create buffer!", -1);
				}

				void* data;
				vkMapMemory(g_context.m_device, stagingBuffer.m_info.deviceMemory, stagingBuffer.m_info.offset, stagingBuffer.m_info.size, 0, &data);
				memcpy(data, vertices.data(), (size_t)bufferSize);
				vkUnmapMemory(g_context.m_device, stagingBuffer.m_info.deviceMemory);
			}

			{
				VkBufferCreateInfo bufferInfo = {};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = bufferSize;
				bufferInfo.usage = usages[i];
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VmaAllocationCreateInfo allocInfo = {};
				allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

				if (vmaCreateBuffer(g_context.m_allocator, &bufferInfo, &allocInfo, &buffers[i]->m_buffer, &buffers[i]->m_allocation, &buffers[i]->m_info) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create buffer!", -1);
				}

				VKUtility::copyBuffer(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, stagingBuffer, *buffers[i], bufferSize);

				vmaDestroyBuffer(g_context.m_allocator, stagingBuffer.m_buffer, stagingBuffer.m_allocation);
			}
		}
	}

	indexCount = indices.size();
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
