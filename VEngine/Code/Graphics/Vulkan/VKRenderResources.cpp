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

VEngine::VKRenderResources::VKRenderResources()
{
}

void VEngine::VKRenderResources::createResizableTextures(unsigned int width, unsigned int height)
{
	// color attachment
	{
		m_colorAttachment.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_colorAttachment.m_format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateImage(g_context.m_allocator, &imageInfo, &allocInfo, &m_colorAttachment.m_image, &m_colorAttachment.m_allocation, nullptr))
		{
			Utility::fatalExit("Failed to create image!", -1);
		}

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_colorAttachment.m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_colorAttachment.m_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_colorAttachment.m_view) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
	}

	// depth attachment
	{
		m_depthAttachment.m_format = VKUtility::findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_depthAttachment.m_format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateImage(g_context.m_allocator, &imageInfo, &allocInfo, &m_depthAttachment.m_image, &m_depthAttachment.m_allocation, nullptr))
		{
			Utility::fatalExit("Failed to create image!", -1);
		}

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_depthAttachment.m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_depthAttachment.m_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_depthAttachment.m_view) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
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
	vmaFreeMemory(g_context.m_allocator, m_colorAttachment.m_allocation);

	vkDestroyImageView(g_context.m_device, m_depthAttachment.m_view, nullptr);
	vkDestroyImage(g_context.m_device, m_depthAttachment.m_image, nullptr);
	vmaFreeMemory(g_context.m_allocator, m_depthAttachment.m_allocation);
}

void VEngine::VKRenderResources::deleteAllTextures()
{
	deleteResizableTextures();
}
