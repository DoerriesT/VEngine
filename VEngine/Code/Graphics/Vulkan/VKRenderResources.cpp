#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "Graphics/RenderParams.h"

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

void VEngine::VKRenderResources::reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize)
{
	if (m_vertexBuffer.m_size < vertexSize || m_indexBuffer.m_size < indexSize)
	{
		if (m_vertexBuffer.m_size > 0 || m_indexBuffer.m_size > 0)
		{
			vkDestroyBuffer(g_context.m_device, m_vertexBuffer.m_buffer, nullptr);
			vkDestroyBuffer(g_context.m_device, m_indexBuffer.m_buffer, nullptr);
			vkFreeMemory(g_context.m_device, m_vertexBuffer.m_memory, nullptr);
		}
		printf("VertexSize: %d\nIndexSize: %d\n", uint32_t(vertexSize), uint32_t(indexSize));
		uint32_t typebits = 0;

		// vertex buffer
		{
			VkBufferCreateInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			vertexBufferInfo.size = vertexSize;
			vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(g_context.m_device, &vertexBufferInfo, nullptr, &m_vertexBuffer.m_buffer) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", -1);
			}

			VkMemoryRequirements vertexBufferMemRequirements;
			vkGetBufferMemoryRequirements(g_context.m_device, m_vertexBuffer.m_buffer, &vertexBufferMemRequirements);
			m_vertexBuffer.m_size = vertexBufferMemRequirements.size;

			if (vertexBufferMemRequirements.alignment > 0)
			{
				m_vertexBuffer.m_size = (m_vertexBuffer.m_size + vertexBufferMemRequirements.alignment - 1) & ~(vertexBufferMemRequirements.alignment - 1);
			}

			typebits |= vertexBufferMemRequirements.memoryTypeBits;
		}

		// index buffer
		{
			VkBufferCreateInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			indexBufferInfo.size = indexSize;
			indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(g_context.m_device, &indexBufferInfo, nullptr, &m_indexBuffer.m_buffer) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", -1);
			}

			VkMemoryRequirements indexBufferMemRequirements;
			vkGetBufferMemoryRequirements(g_context.m_device, m_indexBuffer.m_buffer, &indexBufferMemRequirements);
			m_indexBuffer.m_size = indexBufferMemRequirements.size;

			if (indexBufferMemRequirements.alignment > 0)
			{
				m_indexBuffer.m_size = (m_indexBuffer.m_size + indexBufferMemRequirements.alignment - 1) & ~(indexBufferMemRequirements.alignment - 1);
			}

			typebits |= indexBufferMemRequirements.memoryTypeBits;
		}


		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = m_vertexBuffer.m_size + m_indexBuffer.m_size;
		allocInfo.memoryTypeIndex = VKUtility::findMemoryType(typebits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(g_context.m_device, &allocInfo, nullptr, &m_vertexBuffer.m_memory) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate buffer memory!", -1);
		}

		m_indexBuffer.m_memory = m_vertexBuffer.m_memory;
		vkBindBufferMemory(g_context.m_device, m_vertexBuffer.m_buffer, m_vertexBuffer.m_memory, 0);
		vkBindBufferMemory(g_context.m_device, m_indexBuffer.m_buffer, m_indexBuffer.m_memory, m_vertexBuffer.m_size);
	}
}

void VEngine::VKRenderResources::uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize)
{
	VKBufferData stagingBuffer;
	stagingBuffer.m_size = vertexSize + indexSize;
	VKUtility::createBuffer(stagingBuffer.m_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.m_buffer, stagingBuffer.m_memory);

	void *data;
	vkMapMemory(g_context.m_device, stagingBuffer.m_memory, 0, stagingBuffer.m_size, 0, &data);
	memcpy(data, vertices, (size_t)vertexSize);
	memcpy(((unsigned char *)data) + vertexSize, indices, (size_t)indexSize);
	vkUnmapMemory(g_context.m_device, stagingBuffer.m_memory);

	VkCommandBuffer commandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		VkBufferCopy copyRegionVertex = { 0, 0, vertexSize };
		vkCmdCopyBuffer(commandBuffer, stagingBuffer.m_buffer, m_vertexBuffer.m_buffer, 1, &copyRegionVertex);
		VkBufferCopy copyRegionIndex = { vertexSize, 0, indexSize };
		vkCmdCopyBuffer(commandBuffer, stagingBuffer.m_buffer, m_indexBuffer.m_buffer, 1, &copyRegionIndex);
	}
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, commandBuffer);

	vkDestroyBuffer(g_context.m_device, stagingBuffer.m_buffer, nullptr);
	vkFreeMemory(g_context.m_device, stagingBuffer.m_memory, nullptr);
}

void VEngine::VKRenderResources::createFramebuffer(unsigned int width, unsigned int height, VkRenderPass renderPass)
{
	VkImageView attachments[] = { m_colorAttachment.m_view, m_depthAttachment.m_view };

	VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(VkImageView));
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(g_context.m_device, &framebufferInfo, nullptr, &m_mainFramebuffer) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create framebuffer!", -1);
	}
}

VEngine::VKRenderResources::VKRenderResources()
	:m_vertexBuffer(),
	m_indexBuffer()
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

void VEngine::VKRenderResources::createUniformBuffer(VkDeviceSize perFrameSize, VkDeviceSize perDrawSize)
{

	// Calculate required alignment based on minimum device offset alignment
	VkDeviceSize minUboAlignment = g_context.m_properties.limits.minUniformBufferOffsetAlignment;
	m_perFrameDataSize = perFrameSize;
	m_perDrawDataSize = perDrawSize;

	if (minUboAlignment > 0)
	{
		m_perFrameDataSize = (m_perFrameDataSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
		m_perDrawDataSize = (m_perDrawDataSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	VkDeviceSize bufferSize = MAX_UNIFORM_BUFFER_INSTANCE_COUNT * m_perDrawDataSize + m_perFrameDataSize;

	m_mainUniformBuffer.m_size = bufferSize;
	VKUtility::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_mainUniformBuffer.m_buffer, m_mainUniformBuffer.m_memory);

}

void VEngine::VKRenderResources::createCommandBuffers()
{
	// main
	{
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = g_context.m_graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(g_context.m_device, &allocInfo, &m_mainCommandBuffer) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate command buffer!", -1);
		}
	}

	// forward / blit
	{
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = g_context.m_graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(g_context.m_device, &allocInfo, &m_forwardCommandBuffer) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate command buffers!", -1);
		}
	}

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
