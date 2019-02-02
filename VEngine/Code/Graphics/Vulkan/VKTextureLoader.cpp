#include "VKTextureLoader.h"
#include <cassert>
#include "Utility/ContainerUtility.h"
#include "Utility/Utility.h"
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "VKContext.h"
#include "VKUtility.h"

#define STAGING_BUFFER_SIZE (64ull * 1024 * 1024)

VEngine::VKTextureLoader::VKTextureLoader()
	:m_usedSlots(),
	m_stagingBuffer(),
	m_dummyTexture()
{
	memset(m_textures, 0, sizeof(m_textures));

	// create staging buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = STAGING_BUFFER_SIZE;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		m_stagingBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// create dummy texture
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.extent.width = 1;
		imageCreateInfo.extent.height = 1;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		

		m_dummyTexture.m_image.create(imageCreateInfo, allocCreateInfo);

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
				m_dummyTexture.m_image.getImage(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_dummyTexture.m_image.getImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_dummyTexture.m_image.getFormat();
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_dummyTexture.m_view) != VK_SUCCESS)
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

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_dummyTexture.m_sampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// fill VkDescriptorImageInfo array with dummy textures
	for (size_t i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
	{
		m_descriptorImageInfos[i].sampler = m_dummyTexture.m_sampler;
		m_descriptorImageInfos[i].imageView = m_dummyTexture.m_view;
		m_descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

VEngine::VKTextureLoader::~VKTextureLoader()
{
	// free all remaining textures
	for (size_t i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
	{
		if (m_usedSlots[i])
		{
			free(i + 1);
		}
	}

	// destroy staging buffer
	m_stagingBuffer.destroy();

	// destroy dummy texture
	vkDestroyImageView(g_context.m_device, m_dummyTexture.m_view, nullptr);
	vkDestroySampler(g_context.m_device, m_dummyTexture.m_sampler, nullptr);
	m_dummyTexture.m_image.destroy();
}

size_t VEngine::VKTextureLoader::load(const char *filepath)
{
	// find free id
	size_t id = 0;
	{
		bool foundFreeId = false;

		for (; id < TEXTURE_ARRAY_SIZE; ++id)
		{
			if (!m_usedSlots[id])
			{
				foundFreeId = true;
				break;
			}
		}

		if (!foundFreeId)
		{
			Utility::fatalExit("Ran out of free texture slots!", -1);
		}
	}

	// load texture
	gli::texture2d gliTex(gli::load(filepath));
	{
		if (gliTex.empty())
		{
			Utility::fatalExit(("Failed to load texture: " + std::string(filepath)).c_str(), -1);
		}

		if (gliTex.size() > STAGING_BUFFER_SIZE)
		{
			Utility::fatalExit("Texture is too large for staging buffer!", -1);
		}
	}

	// copy image data to staging buffer
	{
		void *data;
		vmaMapMemory(g_context.m_allocator, m_stagingBuffer.getAllocation(), &data);
		memcpy(data, gliTex.data(), gliTex.size());
		vmaUnmapMemory(g_context.m_allocator, m_stagingBuffer.getAllocation());
	}

	// create image
	VKTexture &texture = m_textures[id];
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VkFormat(gliTex.format());
		imageCreateInfo.extent.width = static_cast<uint32_t>(gliTex.extent().x);
		imageCreateInfo.extent.height = static_cast<uint32_t>(gliTex.extent().y);
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = static_cast<uint32_t>(gliTex.levels());
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;


		texture.m_image.create(imageCreateInfo, allocCreateInfo);
	}

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = texture.m_image.getMipLevels();
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	// copy from staging buffer to image
	{
		std::vector<VkBufferImageCopy> bufferCopyRegions(gliTex.levels());
		size_t offset = 0;

		for (size_t level = 0; level < gliTex.levels(); ++level)
		{
			VkBufferImageCopy &bufferCopyRegion = bufferCopyRegions[level];
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = static_cast<uint32_t>(level);
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(gliTex[level].extent().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(gliTex[level].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			offset += gliTex[level].size();
		}

		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VKUtility::setImageLayout(
				copyCmd,
				texture.m_image.getImage(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				subresourceRange,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

			vkCmdCopyBufferToImage(
				copyCmd,
				m_stagingBuffer.getBuffer(),
				texture.m_image.getImage(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data());

			VKUtility::setImageLayout(
				copyCmd,
				texture.m_image.getImage(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);
	}

	// create image view
	{
		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = texture.m_image.getImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = texture.m_image.getFormat();
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &texture.m_view) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
	}

	// create sampler
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
		samplerCreateInfo.maxLod = static_cast<float>(texture.m_image.getMipLevels());
		samplerCreateInfo.maxAnisotropy = g_context.m_enabledFeatures.samplerAnisotropy ?
			g_context.m_properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = g_context.m_enabledFeatures.samplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &texture.m_sampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// create VkDescriptorImageInfo
	{
		m_descriptorImageInfos[id].sampler = texture.m_sampler;
		m_descriptorImageInfos[id].imageView = texture.m_view;
		m_descriptorImageInfos[id].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	m_usedSlots[id] = true;

	// 0 is reserved as null id
	return id + 1;
}

void VEngine::VKTextureLoader::free(size_t id)
{
	assert(id);

	// 0 is reserved as null id
	--id;

	assert(m_usedSlots[id]);

	VKTexture &texture = m_textures[id];

	// destroy view
	vkDestroyImageView(g_context.m_device, texture.m_view, nullptr);

	// destroy sampler
	vkDestroySampler(g_context.m_device, texture.m_sampler, nullptr);

	// destroy image
	texture.m_image.destroy();
	
	// clear texture
	memset(&texture, 0, sizeof(texture));

	// set VkDescriptorImageInfo to dummy texture
	m_descriptorImageInfos[id].sampler = m_dummyTexture.m_sampler;
	m_descriptorImageInfos[id].imageView = m_dummyTexture.m_view;
	m_descriptorImageInfos[id].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	m_usedSlots[id] = false;
}

void VEngine::VKTextureLoader::getDescriptorImageInfos(const VkDescriptorImageInfo **data, size_t &count)
{
	*data = m_descriptorImageInfos;
	count = TEXTURE_ARRAY_SIZE;
}
