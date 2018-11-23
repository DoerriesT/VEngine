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
