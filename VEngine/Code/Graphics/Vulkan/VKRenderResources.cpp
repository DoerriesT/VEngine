#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "Graphics/RenderParams.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"
#include "Graphics/LightData.h"

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

	descriptorImageInfos[0].sampler = m_shadowSampler;
	descriptorImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfos[0].imageView = m_shadowTexture.m_view;

	for (size_t i = 0; i < TEXTURE_ARRAY_SIZE - 1; ++i)
	{
		if (i < textures.size() && textures[i])
		{
			descriptorImageInfos[i + 1].sampler = textures[i]->m_sampler;
			descriptorImageInfos[i + 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i + 1].imageView = textures[i]->m_imageData.m_view;
		}
		else
		{
			descriptorImageInfos[i + 1].sampler = m_dummySampler;
			descriptorImageInfos[i + 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i + 1].imageView = m_dummyImageView;
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

void VEngine::VKRenderResources::createFramebuffer(unsigned int width, unsigned int height, VkRenderPass geometryFillRenderPass, VkRenderPass shadowRenderPass, VkRenderPass forwardRenderPass)
{
	// geometry fill fbo
	{
		VkImageView attachments[] = { m_depthAttachment.m_view, m_albedoAttachment.m_view, m_normalAttachment.m_view, m_materialAttachment.m_view, m_velocityAttachment.m_view };

		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.renderPass = geometryFillRenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(VkImageView));
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(g_context.m_device, &framebufferInfo, nullptr, &m_geometryFillFramebuffer) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create framebuffer!", -1);
		}
	}

	// shadow fbo
	{
		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.renderPass = shadowRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &m_shadowTexture.m_view;
		framebufferInfo.width = g_shadowAtlasSize;
		framebufferInfo.height = g_shadowAtlasSize;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(g_context.m_device, &framebufferInfo, nullptr, &m_shadowFramebuffer) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create framebuffer!", -1);
		}
	}

	// forward fbo
	{
		VkImageView attachments[] = { m_depthAttachment.m_view, m_velocityAttachment.m_view, m_lightAttachment.m_view };

		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.renderPass = forwardRenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(VkImageView));
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(g_context.m_device, &framebufferInfo, nullptr, &m_forwardFramebuffer) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create framebuffer!", -1);
		}
	}
}

VEngine::VKRenderResources::VKRenderResources()
	:m_vertexBuffer(),
	m_indexBuffer()
{
}

void VEngine::VKRenderResources::createResizableTextures(unsigned int width, unsigned int height)
{
	// depth attachment
	{
		m_depthAttachment.m_format = VKUtility::findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_depthAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthAttachment.m_image, m_depthAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }, m_depthAttachment);
	}

	// albedo attachment
	{
		m_albedoAttachment.m_format = VK_FORMAT_R8G8B8A8_UNORM;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_albedoAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_albedoAttachment.m_image, m_albedoAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_albedoAttachment);
	}

	// normal attachment
	{
		m_normalAttachment.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_normalAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_normalAttachment.m_image, m_normalAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_normalAttachment);
	}

	// material attachment
	{
		m_materialAttachment.m_format = VK_FORMAT_R8G8B8A8_UNORM;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_materialAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_materialAttachment.m_image, m_materialAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_materialAttachment);
	}

	// velocity attachment
	{
		m_velocityAttachment.m_format = VK_FORMAT_R16G16_SFLOAT;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_velocityAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_velocityAttachment.m_image, m_velocityAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_velocityAttachment);
	}

	// light attachment
	{
		m_lightAttachment.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		VKUtility::createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, m_lightAttachment.m_format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_lightAttachment.m_image, m_lightAttachment.m_memory);
		VKUtility::createImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, m_lightAttachment);
	}
}

void VEngine::VKRenderResources::createAllTextures(unsigned int width, unsigned int height)
{
	createResizableTextures(width, height);

	m_shadowTexture.m_format = VK_FORMAT_D16_UNORM;
	VKUtility::createImage(
		g_shadowAtlasSize,
		g_shadowAtlasSize,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		m_shadowTexture.m_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_shadowTexture.m_image,
		m_shadowTexture.m_memory);
	VKUtility::createImageView({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }, m_shadowTexture);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	VkCommandBuffer cmdBuf = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	VKUtility::setImageLayout(
		cmdBuf,
		m_shadowTexture.m_image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, cmdBuf);

	// shadow sampler
	{
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_GREATER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_shadowSampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}
	
	// linear sampler
	{
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

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_linearSampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// point sampler
	{
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
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

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_pointSampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}
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
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		m_mainUniformBuffer.m_buffer,
		m_mainUniformBuffer.m_memory,
		m_mainUniformBuffer.m_size);

	vkMapMemory(g_context.m_device, m_mainUniformBuffer.m_memory, 0, m_mainUniformBuffer.m_size, 0, &m_mainUniformBuffer.m_mapped);
}

void VEngine::VKRenderResources::createStorageBuffers()
{
	// light data buffer
	{
		// Calculate required alignment based on minimum device offset alignment
		VkDeviceSize minSboAlignment = g_context.m_properties.limits.minStorageBufferOffsetAlignment;
		m_directionalLightDataSize = sizeof(DirectionalLightData);
		m_pointLightDataSize = sizeof(PointLightData);
		m_spotLightDataSize = sizeof(SpotLightData);
		m_shadowDataSize = sizeof(ShadowData);

		if (minSboAlignment > 0)
		{
			m_directionalLightDataSize = (m_directionalLightDataSize + minSboAlignment - 1) & ~(minSboAlignment - 1);
			m_pointLightDataSize = (m_pointLightDataSize + minSboAlignment - 1) & ~(minSboAlignment - 1);
			m_spotLightDataSize = (m_spotLightDataSize + minSboAlignment - 1) & ~(minSboAlignment - 1);
			m_shadowDataSize = (m_shadowDataSize + minSboAlignment - 1) & ~(minSboAlignment - 1);
		}

		VkDeviceSize bufferSize =
			MAX_DIRECTIONAL_LIGHTS * m_directionalLightDataSize
			+ MAX_POINT_LIGHTS * m_pointLightDataSize
			+ MAX_SPOT_LIGHTS * m_spotLightDataSize
			+ MAX_SHADOW_DATA * m_shadowDataSize
			+ (MAX_SPOT_LIGHTS + MAX_POINT_LIGHTS) * sizeof(glm::vec4) + sizeof(glm::uvec4) * 2;

		m_lightDataStorageBuffer.m_size = bufferSize;
		VKUtility::createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			m_lightDataStorageBuffer.m_buffer,
			m_lightDataStorageBuffer.m_memory,
			m_lightDataStorageBuffer.m_size);

		vkMapMemory(g_context.m_device, m_lightDataStorageBuffer.m_memory, 0, m_lightDataStorageBuffer.m_size, 0, &m_lightDataStorageBuffer.m_mapped);
	}

	// light index buffer
	{
		uint32_t width = 1600 / 16 + ((1600 % 16 == 0) ? 0 : 1);
		uint32_t height = 900 / 16 + ((900 % 16 == 0) ? 0 : 1);
		uint32_t tileCount = width * height;
		VkDeviceSize bufferSize = (MAX_SPOT_LIGHTS + MAX_POINT_LIGHTS) / 32 * sizeof(uint32_t) * tileCount;

		m_lightIndexStorageBuffer.m_size = bufferSize;
		VKUtility::createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_lightIndexStorageBuffer.m_buffer,
			m_lightIndexStorageBuffer.m_memory,
			m_lightIndexStorageBuffer.m_size);
	}
}

void VEngine::VKRenderResources::createCommandBuffers()
{
	// main
	{
		VkCommandBuffer cmdBufs[] =
		{
			m_tilingCommandBuffer,
			m_geometryFillCommandBuffer,
			m_shadowsCommandBuffer,
			m_lightingCommandBuffer,
			m_forwardCommandBuffer
		};

		VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = g_context.m_graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = sizeof(cmdBufs) / sizeof(cmdBufs[0]);

		if (vkAllocateCommandBuffers(g_context.m_device, &allocInfo, cmdBufs) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate command buffer!", -1);
		}

		m_tilingCommandBuffer = cmdBufs[0];
		m_geometryFillCommandBuffer = cmdBufs[1];
		m_shadowsCommandBuffer = cmdBufs[2];
		m_lightingCommandBuffer = cmdBufs[3];
		m_forwardCommandBuffer = cmdBufs[4];
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

		VKUtility::setImageLayout(
			copyCmd,
			m_lightAttachment.m_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
		// per frame
		{
			VkDescriptorSetLayoutBinding perFrameLayoutBinding = {};
			perFrameLayoutBinding.binding = 0;
			perFrameLayoutBinding.descriptorCount = 1;
			perFrameLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			perFrameLayoutBinding.pImmutableSamplers = nullptr;
			perFrameLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &perFrameLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_perFrameDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// per draw
		{
			VkDescriptorSetLayoutBinding perDrawLayoutBinding = {};
			perDrawLayoutBinding.binding = 0;
			perDrawLayoutBinding.descriptorCount = 1;
			perDrawLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			perDrawLayoutBinding.pImmutableSamplers = nullptr;
			perDrawLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &perDrawLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_perDrawDataDescriptorSetLayout) != VK_SUCCESS)
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

		// lighting input
		{
			VkDescriptorSetLayoutBinding lightingInputLayoutBindings[2] = {};

			// result image
			lightingInputLayoutBindings[0].binding = 0;
			lightingInputLayoutBindings[0].descriptorCount = 1;
			lightingInputLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			lightingInputLayoutBindings[0].pImmutableSamplers = nullptr;
			lightingInputLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// gbuffer textures
			lightingInputLayoutBindings[1].binding = 1;
			lightingInputLayoutBindings[1].descriptorCount = 4;
			lightingInputLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			lightingInputLayoutBindings[1].pImmutableSamplers = nullptr;
			lightingInputLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightingInputLayoutBindings) / sizeof(lightingInputLayoutBindings[0]);
			layoutInfo.pBindings = lightingInputLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightingInputDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// light data
		{
			VkDescriptorSetLayoutBinding lightDataLayoutBindings[5] = {};

			// directional light data
			lightDataLayoutBindings[0].binding = 0;
			lightDataLayoutBindings[0].descriptorCount = 1;
			lightDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[0].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// point light data
			lightDataLayoutBindings[1].binding = 1;
			lightDataLayoutBindings[1].descriptorCount = 1;
			lightDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[1].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// spot light data
			lightDataLayoutBindings[2].binding = 2;
			lightDataLayoutBindings[2].descriptorCount = 1;
			lightDataLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[2].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// shadow data
			lightDataLayoutBindings[3].binding = 3;
			lightDataLayoutBindings[3].descriptorCount = 1;
			lightDataLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[3].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// shadow texture
			lightDataLayoutBindings[4].binding = 4;
			lightDataLayoutBindings[4].descriptorCount = 1;
			lightDataLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			lightDataLayoutBindings[4].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightDataLayoutBindings) / sizeof(lightDataLayoutBindings[0]);
			layoutInfo.pBindings = lightDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// cull data
		{
			VkDescriptorSetLayoutBinding cullDataLayoutBindings[2] = {};

			// point light data
			cullDataLayoutBindings[0].binding = 0;
			cullDataLayoutBindings[0].descriptorCount = 1;
			cullDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			cullDataLayoutBindings[0].pImmutableSamplers = nullptr;
			cullDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// spot light data
			cullDataLayoutBindings[1].binding = 1;
			cullDataLayoutBindings[1].descriptorCount = 1;
			cullDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			cullDataLayoutBindings[1].pImmutableSamplers = nullptr;
			cullDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(cullDataLayoutBindings) / sizeof(cullDataLayoutBindings[0]);
			layoutInfo.pBindings = cullDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_cullDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// light index data
		{
			VkDescriptorSetLayoutBinding lightIndexDataLayoutBindings[2] = {};

			// point light indices
			lightIndexDataLayoutBindings[0].binding = 0;
			lightIndexDataLayoutBindings[0].descriptorCount = 1;
			lightIndexDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightIndexDataLayoutBindings[0].pImmutableSamplers = nullptr;
			lightIndexDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// spot light indices
			lightIndexDataLayoutBindings[1].binding = 1;
			lightIndexDataLayoutBindings[1].descriptorCount = 1;
			lightIndexDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightIndexDataLayoutBindings[1].pImmutableSamplers = nullptr;
			lightIndexDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightIndexDataLayoutBindings) / sizeof(lightIndexDataLayoutBindings[0]);
			layoutInfo.pBindings = lightIndexDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightIndexDescriptorSetLayout) != VK_SUCCESS)
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
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , TEXTURE_ARRAY_SIZE + 1 + 4 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
		};

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 7;

		if (vkCreateDescriptorPool(g_context.m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create descriptor pool!", -1);
		}
	}

	// create descriptor sets
	{
		VkDescriptorSetLayout layouts[] =
		{
			m_perFrameDataDescriptorSetLayout,
			m_perDrawDataDescriptorSetLayout,
			m_textureDescriptorSetLayout,
			m_lightingInputDescriptorSetLayout,
			m_lightDataDescriptorSetLayout,
			m_cullDataDescriptorSetLayout,
			m_lightIndexDescriptorSetLayout
		};

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
		allocInfo.pSetLayouts = layouts;

		VkDescriptorSet sets[] =
		{
			m_perFrameDataDescriptorSet,
			m_perDrawDataDescriptorSet ,
			m_textureDescriptorSet,
			m_lightingInputDescriptorSet,
			m_lightDataDescriptorSet,
			m_cullDataDescriptorSet,
			m_lightIndexDescriptorSet
		};

		if (vkAllocateDescriptorSets(g_context.m_device, &allocInfo, sets) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate descriptor set!", -1);
		}

		m_perFrameDataDescriptorSet = sets[0];
		m_perDrawDataDescriptorSet = sets[1];
		m_textureDescriptorSet = sets[2];
		m_lightingInputDescriptorSet = sets[3];
		m_lightDataDescriptorSet = sets[4];
		m_cullDataDescriptorSet = sets[5];
		m_lightIndexDescriptorSet = sets[6];

		// per frame UBO
		VkDescriptorBufferInfo perFrameBufferInfo = {};
		perFrameBufferInfo.buffer = m_mainUniformBuffer.m_buffer;
		perFrameBufferInfo.offset = 0;
		perFrameBufferInfo.range = sizeof(RenderParams);

		// per draw UBO
		VkDescriptorBufferInfo perDrawBufferInfo = {};
		perDrawBufferInfo.buffer = m_mainUniformBuffer.m_buffer;
		perDrawBufferInfo.offset = m_perFrameDataSize;
		perDrawBufferInfo.range = m_perDrawDataSize;

		// textures
		VkDescriptorImageInfo descriptorImageInfos[TEXTURE_ARRAY_SIZE];

		for (size_t i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
		{
			descriptorImageInfos[i].sampler = m_dummySampler;
			descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfos[i].imageView = m_dummyImageView;
		}

		// lighting result image
		VkDescriptorImageInfo lightingResultDescriptorImageInfo;
		lightingResultDescriptorImageInfo.sampler = VK_NULL_HANDLE;
		lightingResultDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		lightingResultDescriptorImageInfo.imageView = m_lightAttachment.m_view;

		VkDescriptorImageInfo lightingInputdescriptorImageInfos[4];

		// depth
		lightingInputdescriptorImageInfos[0].sampler = m_pointSampler;
		lightingInputdescriptorImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		lightingInputdescriptorImageInfos[0].imageView = m_depthAttachment.m_view;

		// albedo
		lightingInputdescriptorImageInfos[1].sampler = m_linearSampler;
		lightingInputdescriptorImageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		lightingInputdescriptorImageInfos[1].imageView = m_albedoAttachment.m_view;

		// normal
		lightingInputdescriptorImageInfos[2].sampler = m_linearSampler;
		lightingInputdescriptorImageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		lightingInputdescriptorImageInfos[2].imageView = m_normalAttachment.m_view;

		// material
		lightingInputdescriptorImageInfos[3].sampler = m_linearSampler;
		lightingInputdescriptorImageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		lightingInputdescriptorImageInfos[3].imageView = m_materialAttachment.m_view;

		// directional light data
		VkDescriptorBufferInfo directionalLightDataDescriptorBufferInfo = {};
		directionalLightDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		directionalLightDataDescriptorBufferInfo.offset = 0;
		directionalLightDataDescriptorBufferInfo.range = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS;

		// point light data
		VkDescriptorBufferInfo pointLightDataDescriptorBufferInfo = {};
		pointLightDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		pointLightDataDescriptorBufferInfo.offset = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS;
		pointLightDataDescriptorBufferInfo.range = m_pointLightDataSize * MAX_POINT_LIGHTS;

		// spot light data
		VkDescriptorBufferInfo spotLightDataDescriptorBufferInfo = {};
		spotLightDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		spotLightDataDescriptorBufferInfo.offset = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS + m_pointLightDataSize * MAX_POINT_LIGHTS;
		spotLightDataDescriptorBufferInfo.range = m_spotLightDataSize * MAX_SPOT_LIGHTS;

		// shadow data
		VkDescriptorBufferInfo shadowDataDescriptorBufferInfo = {};
		shadowDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		shadowDataDescriptorBufferInfo.offset = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS + m_pointLightDataSize * MAX_POINT_LIGHTS + m_spotLightDataSize * MAX_SPOT_LIGHTS;
		shadowDataDescriptorBufferInfo.range = m_shadowDataSize * MAX_SHADOW_DATA;

		// shadow texture
		VkDescriptorImageInfo shadowTextureDescriptorImageInfo = {};
		shadowTextureDescriptorImageInfo.sampler = m_shadowSampler;
		shadowTextureDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowTextureDescriptorImageInfo.imageView = m_shadowTexture.m_view;

		// point light cull data
		VkDescriptorBufferInfo pointLightCullDataDescriptorBufferInfo = {};
		pointLightCullDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		pointLightCullDataDescriptorBufferInfo.offset = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS + m_pointLightDataSize * MAX_POINT_LIGHTS + m_spotLightDataSize * MAX_SPOT_LIGHTS + m_shadowDataSize * MAX_SHADOW_DATA;
		pointLightCullDataDescriptorBufferInfo.range = sizeof(glm::vec4) * MAX_POINT_LIGHTS;

		// spot light cull data
		VkDescriptorBufferInfo spotLightCullDataDescriptorBufferInfo = {};
		spotLightCullDataDescriptorBufferInfo.buffer = m_lightDataStorageBuffer.m_buffer;
		spotLightCullDataDescriptorBufferInfo.offset = m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS + m_pointLightDataSize * MAX_POINT_LIGHTS + m_spotLightDataSize * MAX_SPOT_LIGHTS + m_shadowDataSize * MAX_SHADOW_DATA + sizeof(glm::vec4) * MAX_POINT_LIGHTS;
		spotLightCullDataDescriptorBufferInfo.range = sizeof(glm::vec4) * MAX_SPOT_LIGHTS;

		uint32_t width = 1600 / 16 + ((1600 % 16 == 0) ? 0 : 1);
		uint32_t height = 900 / 16 + ((900 % 16 == 0) ? 0 : 1);
		uint32_t tileCount = width * height;

		// point light indices
		VkDescriptorBufferInfo pointLightIndicesDescriptorBufferInfo = {};
		pointLightIndicesDescriptorBufferInfo.buffer = m_lightIndexStorageBuffer.m_buffer;
		pointLightIndicesDescriptorBufferInfo.offset = 0;
		pointLightIndicesDescriptorBufferInfo.range = sizeof(uint32_t) * MAX_POINT_LIGHTS / 32 * tileCount;

		// spot light indices
		VkDescriptorBufferInfo spotLightIndicesDescriptorBufferInfo = {};
		spotLightIndicesDescriptorBufferInfo.buffer = m_lightIndexStorageBuffer.m_buffer;
		spotLightIndicesDescriptorBufferInfo.offset = pointLightIndicesDescriptorBufferInfo.range;
		spotLightIndicesDescriptorBufferInfo.range = sizeof(uint32_t) * MAX_SPOT_LIGHTS / 32 * tileCount;

		VkWriteDescriptorSet descriptorWrites[14] = {};
		{
			// per frame UBO
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_perFrameDataDescriptorSet;
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &perFrameBufferInfo;

			// per draw UBO
			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = m_perDrawDataDescriptorSet;
			descriptorWrites[1].dstBinding = 0;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pBufferInfo = &perDrawBufferInfo;

			// textures
			descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[2].dstSet = m_textureDescriptorSet;
			descriptorWrites[2].dstBinding = 0;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[2].descriptorCount = TEXTURE_ARRAY_SIZE;
			descriptorWrites[2].pImageInfo = descriptorImageInfos;

			// lighting result
			descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[3].dstSet = m_lightingInputDescriptorSet;
			descriptorWrites[3].dstBinding = 0;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].pImageInfo = &lightingResultDescriptorImageInfo;

			// lighting inputs
			descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[4].dstSet = m_lightingInputDescriptorSet;
			descriptorWrites[4].dstBinding = 1;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[4].descriptorCount = 4;
			descriptorWrites[4].pImageInfo = lightingInputdescriptorImageInfos;

			// directional light data
			descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[5].dstSet = m_lightDataDescriptorSet;
			descriptorWrites[5].dstBinding = 0;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].pBufferInfo = &directionalLightDataDescriptorBufferInfo;

			// point light data
			descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[6].dstSet = m_lightDataDescriptorSet;
			descriptorWrites[6].dstBinding = 1;
			descriptorWrites[6].dstArrayElement = 0;
			descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[6].descriptorCount = 1;
			descriptorWrites[6].pBufferInfo = &pointLightDataDescriptorBufferInfo;

			// spot light data
			descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[7].dstSet = m_lightDataDescriptorSet;
			descriptorWrites[7].dstBinding = 2;
			descriptorWrites[7].dstArrayElement = 0;
			descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[7].descriptorCount = 1;
			descriptorWrites[7].pBufferInfo = &spotLightDataDescriptorBufferInfo;

			// shadow data
			descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[8].dstSet = m_lightDataDescriptorSet;
			descriptorWrites[8].dstBinding = 3;
			descriptorWrites[8].dstArrayElement = 0;
			descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[8].descriptorCount = 1;
			descriptorWrites[8].pBufferInfo = &shadowDataDescriptorBufferInfo;

			// shadow texture
			descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[9].dstSet = m_lightDataDescriptorSet;
			descriptorWrites[9].dstBinding = 4;
			descriptorWrites[9].dstArrayElement = 0;
			descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[9].descriptorCount = 1;
			descriptorWrites[9].pImageInfo = &shadowTextureDescriptorImageInfo;

			// point light cull data
			descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[10].dstSet = m_cullDataDescriptorSet;
			descriptorWrites[10].dstBinding = 0;
			descriptorWrites[10].dstArrayElement = 0;
			descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[10].descriptorCount = 1;
			descriptorWrites[10].pBufferInfo = &pointLightCullDataDescriptorBufferInfo;

			// spot light cull data
			descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[11].dstSet = m_cullDataDescriptorSet;
			descriptorWrites[11].dstBinding = 1;
			descriptorWrites[11].dstArrayElement = 0;
			descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[11].descriptorCount = 1;
			descriptorWrites[11].pBufferInfo = &spotLightCullDataDescriptorBufferInfo;

			// point light indices
			descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[12].dstSet = m_lightIndexDescriptorSet;
			descriptorWrites[12].dstBinding = 0;
			descriptorWrites[12].dstArrayElement = 0;
			descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[12].descriptorCount = 1;
			descriptorWrites[12].pBufferInfo = &pointLightIndicesDescriptorBufferInfo;

			// spot light indices
			descriptorWrites[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[13].dstSet = m_lightIndexDescriptorSet;
			descriptorWrites[13].dstBinding = 1;
			descriptorWrites[13].dstArrayElement = 0;
			descriptorWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[13].descriptorCount = 1;
			descriptorWrites[13].pBufferInfo = &spotLightIndicesDescriptorBufferInfo;
		}

		vkUpdateDescriptorSets(g_context.m_device, static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
	}
}

void VEngine::VKRenderResources::createEvents()
{
	VkEventCreateInfo createInfo = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	vkCreateEvent(g_context.m_device, &createInfo, nullptr, &m_shadowsFinishedEvent);
}

void VEngine::VKRenderResources::deleteResizableTextures()
{
	//vkDestroyImageView(g_context.m_device, m_colorAttachment.m_view, nullptr);
	//vkDestroyImage(g_context.m_device, m_colorAttachment.m_image, nullptr);
	//vkFreeMemory(g_context.m_device, m_colorAttachment.m_memory, nullptr);
	//
	//vkDestroyImageView(g_context.m_device, m_depthAttachment.m_view, nullptr);
	//vkDestroyImage(g_context.m_device, m_depthAttachment.m_image, nullptr);
	//vkFreeMemory(g_context.m_device, m_depthAttachment.m_memory, nullptr);
}

void VEngine::VKRenderResources::deleteAllTextures()
{
	deleteResizableTextures();
}
