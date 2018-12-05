#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "Graphics/RenderParams.h"
#include "VKTextureLoader.h"

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
	VKUtility::createBuffer(
		stagingBuffer.m_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer.m_buffer,
		stagingBuffer.m_memory,
		stagingBuffer.m_size);

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

void VEngine::VKRenderResources::updateTextureArray(const std::vector<VKTexture *> &textures)
{
	VkDescriptorImageInfo descriptorImageInfos[TEXTURE_ARRAY_SIZE];

	for (size_t i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
	{
		if (i < textures.size() && textures[i])
		{
			descriptorImageInfos[i].sampler = textures[i]->m_sampler;
			descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i].imageView = textures[i]->m_imageData.m_view;
		}
		else
		{
			descriptorImageInfos[i].sampler = m_dummySampler;
			descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i].imageView = m_dummyImageView;
		}
	}

	VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = m_textureDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = TEXTURE_ARRAY_SIZE;
	descriptorWrite.pImageInfo = descriptorImageInfos;

	vkQueueWaitIdle(g_context.m_graphicsQueue);
	vkUpdateDescriptorSets(g_context.m_device, 1, &descriptorWrite, 0, nullptr);
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
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_colorAttachment);
	}

	// depth attachment
	{
		m_depthAttachment.m_format = VKUtility::findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_depthAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthAttachment.m_image, m_depthAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }, m_depthAttachment);
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
	VKUtility::createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		m_mainUniformBuffer.m_buffer,
		m_mainUniformBuffer.m_memory,
		m_mainUniformBuffer.m_size);

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

	// prepass / forward 
	{
		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = g_context.m_graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandBufferCount = 3;

		VkCommandBuffer cmdBufs[] = { m_depthPrepassCommandBuffer, m_depthPrepassAlphaMaskCommandBuffer, m_forwardCommandBuffer };

		if (vkAllocateCommandBuffers(g_context.m_device, &allocInfo, cmdBufs) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate command buffers!", -1);
		}

		m_depthPrepassCommandBuffer = cmdBufs[0];
		m_depthPrepassAlphaMaskCommandBuffer = cmdBufs[1];
		m_forwardCommandBuffer = cmdBufs[2];
	}

}

void VEngine::VKRenderResources::createDummyTexture()
{
	VKUtility::createImage(
		1,
		1,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_dummyImage,
		m_dummyMemory);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		VKUtility::setImageLayout(
			copyCmd,
			m_dummyImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = m_dummyImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange = subresourceRange;

	if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_dummyImageView) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create image view!", -1);
	}

	VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_dummySampler) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create sampler!", -1);
	}
}

void VEngine::VKRenderResources::createDescriptors()
{
	// create descriptor set layouts
	{
		// entity data descriptor set layout
		{
			VkDescriptorSetLayoutBinding perFrameLayoutBinding = {};
			perFrameLayoutBinding.binding = 0;
			perFrameLayoutBinding.descriptorCount = 1;
			perFrameLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			perFrameLayoutBinding.pImmutableSamplers = nullptr;
			perFrameLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutBinding perDrawLayoutBinding = {};
			perDrawLayoutBinding.binding = 1;
			perDrawLayoutBinding.descriptorCount = 1;
			perDrawLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			perDrawLayoutBinding.pImmutableSamplers = nullptr;
			perDrawLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutBinding bindings[] = { perFrameLayoutBinding, perDrawLayoutBinding };

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_entityDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// texture array descriptor set layout
		{
			VkDescriptorSetLayoutBinding textureArrayLayoutBinding = {};
			textureArrayLayoutBinding.binding = 0;
			textureArrayLayoutBinding.descriptorCount = TEXTURE_ARRAY_SIZE;
			textureArrayLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			textureArrayLayoutBinding.pImmutableSamplers = nullptr;
			textureArrayLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &textureArrayLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}
	}

	// create descriptor set pool
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC , 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , TEXTURE_ARRAY_SIZE }
		};

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 2;

		if (vkCreateDescriptorPool(g_context.m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create descriptor pool!", -1);
		}
	}

	// create descriptor sets
	{
		VkDescriptorSetLayout layouts[] = { m_entityDataDescriptorSetLayout, m_textureDescriptorSetLayout };

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
		allocInfo.pSetLayouts = layouts;

		VkDescriptorSet sets[] = { m_entityDataDescriptorSet , m_textureDescriptorSet };

		if (vkAllocateDescriptorSets(g_context.m_device, &allocInfo, sets) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate descriptor set!", -1);
		}

		m_entityDataDescriptorSet = sets[0];
		m_textureDescriptorSet = sets[1];

		VkDescriptorBufferInfo perFrameBufferInfo = {};
		perFrameBufferInfo.buffer = m_mainUniformBuffer.m_buffer;
		perFrameBufferInfo.offset = 0;
		perFrameBufferInfo.range = sizeof(RenderParams);

		VkDescriptorBufferInfo perDrawBufferInfo = {};
		perDrawBufferInfo.buffer = m_mainUniformBuffer.m_buffer;
		perDrawBufferInfo.offset = m_perFrameDataSize;
		perDrawBufferInfo.range = m_perDrawDataSize;

		VkDescriptorImageInfo descriptorImageInfos[TEXTURE_ARRAY_SIZE];

		for (size_t i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
		{
			descriptorImageInfos[i].sampler = m_dummySampler;
			descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i].imageView = m_dummyImageView;
		}

		VkWriteDescriptorSet descriptorWrites[3] = {};
		{
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_entityDataDescriptorSet;
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &perFrameBufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = m_entityDataDescriptorSet;
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pBufferInfo = &perDrawBufferInfo;

			descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[2].dstSet = m_textureDescriptorSet;
			descriptorWrites[2].dstBinding = 0;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[2].descriptorCount = TEXTURE_ARRAY_SIZE;
			descriptorWrites[2].pImageInfo = descriptorImageInfos;
		}

		vkUpdateDescriptorSets(g_context.m_device, static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
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
