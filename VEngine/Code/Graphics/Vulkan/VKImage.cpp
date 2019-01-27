#include "VKImage.h"
#include "VKContext.h"
#include "Utility/Utility.h"

VEngine::VKImage::VKImage(const VkImageCreateInfo &imageCreateInfo, const VmaAllocationCreateInfo &allocCreateInfo)
	:m_imageType(imageCreateInfo.imageType),
	m_format(imageCreateInfo.format),
	m_width(imageCreateInfo.extent.width),
	m_height(imageCreateInfo.extent.height),
	m_depth(imageCreateInfo.extent.depth),
	m_mipLevels(imageCreateInfo.mipLevels),
	m_arrayLayers(imageCreateInfo.arrayLayers),
	m_samples(imageCreateInfo.samples),
	m_tiling(imageCreateInfo.tiling),
	m_sharingMode(imageCreateInfo.sharingMode)
{
	if (vmaCreateImage(g_context.m_allocator, &imageCreateInfo, &allocCreateInfo, &m_image, &m_allocation, &m_allocationInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create image!", -1);
	}
}

VEngine::VKImage::~VKImage()
{
	vmaDestroyImage(g_context.m_allocator, m_image, m_allocation);
}