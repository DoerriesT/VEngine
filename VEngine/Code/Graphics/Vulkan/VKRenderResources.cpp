#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"

extern VEngine::VKContext g_context;


VEngine::VKRenderResources::~VKRenderResources()
{
	deleteAllTextures();
}

void VEngine::VKRenderResources::init(unsigned int width, unsigned int height)
{
	createAllTextures(width, height);
}

void VEngine::VKRenderResources::resize(unsigned int width, unsigned int height)
{
	deleteResizableTextures();
	createResizableTextures(width, height);
}

void VEngine::VKRenderResources::reserveMeshBuffer(uint64_t size)
{
	if (m_meshBuffer.m_size < size)
	{
		if (m_meshBuffer.m_size > 0)
		{
			vkDestroyBuffer(g_context.m_device, m_meshBuffer.m_buffer, nullptr);
			vkFreeMemory(g_context.m_device, m_meshBuffer.m_memory, nullptr);
		}

		m_meshBuffer.m_size = size;
		VKUtility::createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_meshBuffer.m_buffer, m_meshBuffer.m_memory);
	}
}

void VEngine::VKRenderResources::uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize)
{
	VKBufferData stagingBuffer;
	stagingBuffer.m_size = vertexSize + indexSize;
	VKUtility::createBuffer(stagingBuffer.m_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.m_buffer, stagingBuffer.m_memory);

	void *data;
	vkMapMemory(g_context.m_device, stagingBuffer.m_memory, 0, stagingBuffer.m_size, 0, &data);
	memcpy(data, vertices, (size_t)stagingBuffer.m_size);
	vkUnmapMemory(g_context.m_device, stagingBuffer.m_memory);

	VKUtility::copyBuffer(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, stagingBuffer, m_meshBuffer, stagingBuffer.m_size);

	vkDestroyBuffer(g_context.m_device, stagingBuffer.m_buffer, nullptr);
	vkFreeMemory(g_context.m_device, stagingBuffer.m_memory, nullptr);
}

VEngine::VKRenderResources::VKRenderResources()
	:m_meshBuffer()
{
}

void VEngine::VKRenderResources::createResizableTextures(unsigned int width, unsigned int height)
{
	// color attachment
	{
		m_colorAttachment.m_format = VK_FORMAT_R8G8B8A8_UNORM;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_colorAttachment.m_format, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_colorAttachment.m_image, m_colorAttachment.m_memory);
		VKUtility::createImageView(VK_IMAGE_ASPECT_COLOR_BIT, m_colorAttachment);
	}

	// depth attachment
	{
		m_depthAttachment.m_format = VKUtility::findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_depthAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthAttachment.m_image, m_depthAttachment.m_memory);
		VKUtility::createImageView(VK_IMAGE_ASPECT_DEPTH_BIT, m_depthAttachment);
	}
}

void VEngine::VKRenderResources::createAllTextures(unsigned int width, unsigned int height)
{
	createResizableTextures(width, height);
}

void VEngine::VKRenderResources::deleteResizableTextures()
{
	vkDestroyImageView(g_context.m_device, m_colorAttachment.m_view, nullptr);
	vkDestroyImage(g_context.m_device, m_colorAttachment.m_image, nullptr);
	vkFreeMemory(g_context.m_device, m_colorAttachment.m_memory, nullptr);

	vkDestroyImageView(g_context.m_device, m_depthAttachment.m_view, nullptr);
	vkDestroyImage(g_context.m_device, m_depthAttachment.m_image, nullptr);
	vkFreeMemory(g_context.m_device, m_depthAttachment.m_memory, nullptr);
}

void VEngine::VKRenderResources::deleteAllTextures()
{
	deleteResizableTextures();
}
