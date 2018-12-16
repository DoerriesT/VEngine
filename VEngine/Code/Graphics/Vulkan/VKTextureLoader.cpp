#include "VKTextureLoader.h"
#include <cassert>
#include "Utility/ContainerUtility.h"
#include "Utility/Utility.h"
#include <stb_image.h>
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "VKContext.h"
#include "VKUtility.h"

VEngine::VKTextureLoader::VKTextureLoader()
	:m_nextId()
{
}

uint32_t VEngine::VKTextureLoader::load(const char *filepath)
{
	uint32_t id;
	if (m_freeIds.empty())
	{
		// we skip id 0 as it represents the absence of a texture
		id = ++m_nextId;
	}
	else
	{
		id = m_freeIds.back();
		m_freeIds.pop_back();
	}

	bool isPNG = Utility::getFileExtension(filepath) == "png";

	if (isPNG)
	{
		assert(false);
	}
	else
	{
		gli::texture2d gliTex(gli::load(filepath));

		if (gliTex.empty())
		{
			Utility::fatalExit(("Failed to load texture: " + std::string(filepath)).c_str(), -1);
		}

		VKTexture texture = {};
		texture.m_id = id;
		texture.m_imageData.m_format = VkFormat(gliTex.format());
		texture.m_width = static_cast<unsigned int>(gliTex.extent().x);
		texture.m_height = static_cast<unsigned int>(gliTex.extent().y);
		texture.m_layers = static_cast<unsigned int>(gliTex.layers());
		texture.m_levels = static_cast<unsigned int>(gliTex.levels());

		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		VkMemoryRequirements memReqs = {};

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		VkDeviceSize allocSize = 0;

		VKUtility::createBuffer(gliTex.size(),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory, allocSize);

		void *data;
		vkMapMemory(g_context.m_device, stagingMemory, 0, allocSize, 0, &data);
		memcpy(data, gliTex.data(), gliTex.size());
		vkUnmapMemory(g_context.m_device, stagingMemory);

		std::vector<VkBufferImageCopy> bufferCopyRegions;
		size_t offset = 0;

		for (size_t level = 0; level < gliTex.levels(); ++level)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = static_cast<uint32_t>(level);
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(gliTex[level].extent().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(gliTex[level].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			offset += gliTex[level].size();
		}

		VKUtility::createImage(
			texture.m_width,
			texture.m_height,
			texture.m_levels,
			VK_SAMPLE_COUNT_1_BIT,
			texture.m_imageData.m_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture.m_imageData.m_image,
			texture.m_imageData.m_memory);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = texture.m_levels;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;

		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VKUtility::setImageLayout(
				copyCmd, 
				texture.m_imageData.m_image,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
				subresourceRange, 
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

			vkCmdCopyBufferToImage(
				copyCmd, 
				stagingBuffer, 
				texture.m_imageData.m_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data());

			VKUtility::setImageLayout(
				copyCmd,
				texture.m_imageData.m_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

		vkFreeMemory(g_context.m_device, stagingMemory, nullptr);
		vkDestroyBuffer(g_context.m_device, stagingBuffer, nullptr);

		VKUtility::createImageView(subresourceRange, texture.m_imageData);

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
		samplerCreateInfo.maxLod = static_cast<float>(texture.m_levels);
		samplerCreateInfo.maxAnisotropy = g_context.m_enabledFeatures.samplerAnisotropy ? g_context.m_properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = g_context.m_enabledFeatures.samplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &texture.m_sampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}

		m_idToTexture[id] = texture;
	}

	return id;
}

void VEngine::VKTextureLoader::free(uint32_t id)
{
	assert(ContainerUtility::contains(m_idToTexture, id));

	VKTexture &texture = m_idToTexture[id];


	ContainerUtility::remove(m_idToTexture, id);
	m_freeIds.push_back(id);
}

std::vector<VEngine::VKTexture *> VEngine::VKTextureLoader::getTextures()
{
	if (m_idToTexture.size() == 0)
	{
		return {};
	}
	std::vector<VKTexture *> textures(m_idToTexture.rbegin()->first);

	for (auto &p : m_idToTexture)
	{
		textures[p.first - 1] = &p.second;
	}

	return textures;
}
