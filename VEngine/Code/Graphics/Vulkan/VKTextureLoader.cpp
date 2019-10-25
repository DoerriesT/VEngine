#include "VKTextureLoader.h"
#include <cassert>
#include "Utility/ContainerUtility.h"
#include "Utility/Utility.h"
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "VKContext.h"
#include "VKUtility.h"

VEngine::VKTextureLoader::VKTextureLoader(VKBuffer &stagingBuffer)
	:m_freeHandles(new TextureHandle[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_freeHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
	m_stagingBuffer(stagingBuffer)
{
	memset(m_views, 0, sizeof(m_views));
	memset(m_images, 0, sizeof(m_images));
	memset(m_allocationHandles, 0, sizeof(m_allocationHandles));

	for (TextureHandle i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		m_freeHandles[i] = RendererConsts::TEXTURE_ARRAY_SIZE - i;
	}

	// create dummy texture
	{
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
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

		VKAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_images[0], m_allocationHandles[0]);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;

		// change layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		VKUtility::setImageLayout(copyCmd, m_images[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

		VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_images[0];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = imageCreateInfo.format;
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_views[0]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", EXIT_FAILURE);
		}
	}

	// fill VkDescriptorImageInfo array with dummy textures
	for (size_t i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		m_descriptorImageInfos[i].sampler = VK_NULL_HANDLE;
		m_descriptorImageInfos[i].imageView = m_views[0];
		m_descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

uint32_t allocatedCount = 0;

VEngine::VKTextureLoader::~VKTextureLoader()
{
	// free all remaining textures
	for (TextureHandle i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		if (m_descriptorImageInfos[i].imageView != m_views[0])
		{
			free(i + 1);
		}
	}

	// destroy dummy texture
	vkDestroyImageView(g_context.m_device, m_views[0], nullptr);
	g_context.m_allocator.destroyImage(m_images[0], m_allocationHandles[0]);
}

VEngine::TextureHandle VEngine::VKTextureLoader::load(const char *filepath)
{
	// acquire handle
	TextureHandle handle = 0;
	{
		if (!m_freeHandleCount)
		{
			Utility::fatalExit("Out of Texture Handles!", EXIT_FAILURE);
		}

		--m_freeHandleCount;
		handle = m_freeHandles[m_freeHandleCount];
		++allocatedCount;
	}

	// load texture
	gli::texture2d gliTex(gli::load(filepath));
	{
		if (gliTex.empty())
		{
			Utility::fatalExit(("Failed to load texture: " + std::string(filepath)).c_str(), -1);
		}

		if (gliTex.size() > RendererConsts::STAGING_BUFFER_SIZE)
		{
			Utility::fatalExit("Texture is too large for staging buffer!", EXIT_FAILURE);
		}
	}

	// copy image data to staging buffer
	{
		void *data;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), &data);
		memcpy(data, gliTex.data(), gliTex.size());
		g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());
	}

	// create image
	{
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
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

		VKAllocationCreateInfo allocCreateInfo{};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_images[handle], m_allocationHandles[handle]);
	}

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = static_cast<uint32_t>(gliTex.levels());
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

		// copy to image and transition layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VKUtility::setImageLayout(copyCmd, m_images[handle], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vkCmdCopyBufferToImage(copyCmd, m_stagingBuffer.getBuffer(), m_images[handle], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			VKUtility::setImageLayout(copyCmd, m_images[handle], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);
	}

	// create image view
	{
		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_images[handle];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VkFormat(gliTex.format());
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_views[handle]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
	}

	// create VkDescriptorImageInfo
	{
		// valid handles start from 1, but we also want to use the first element in m_descriptorImageInfos too, so subtract 1
		m_descriptorImageInfos[handle - 1].sampler = VK_NULL_HANDLE;
		m_descriptorImageInfos[handle - 1].imageView = m_views[handle];
		m_descriptorImageInfos[handle - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	return handle;
}

void VEngine::VKTextureLoader::free(TextureHandle handle)
{
	// free handle
	assert(m_freeHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_freeHandles[m_freeHandleCount] = handle;
	++m_freeHandleCount;
	--allocatedCount;

	// destroy resources
	vkDestroyImageView(g_context.m_device, m_views[handle], nullptr);
	g_context.m_allocator.destroyImage(m_images[handle], m_allocationHandles[handle]);

	// clear texture
	m_views[handle] = VK_NULL_HANDLE;
	m_images[handle] = VK_NULL_HANDLE;
	m_allocationHandles[handle] = nullptr;

	// set VkDescriptorImageInfo to dummy texture
	// valid handles start from 1, but we also want to use the first element in m_descriptorImageInfos too, so subtract 1
	m_descriptorImageInfos[handle - 1].sampler = VK_NULL_HANDLE;
	m_descriptorImageInfos[handle - 1].imageView = m_views[0];
	m_descriptorImageInfos[handle - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VEngine::VKTextureLoader::getDescriptorImageInfos(const VkDescriptorImageInfo **data, size_t &count)
{
	*data = m_descriptorImageInfos;
	count = RendererConsts::TEXTURE_ARRAY_SIZE;
}
