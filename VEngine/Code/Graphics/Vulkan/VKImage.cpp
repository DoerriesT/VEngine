#include "VKImage.h"
#include "VKContext.h"
#include "Utility/Utility.h"

void VEngine::VKImage::create(const VkImageCreateInfo & imageCreateInfo, const VmaAllocationCreateInfo & allocCreateInfo)
{
	m_imageType = imageCreateInfo.imageType;
	m_format = imageCreateInfo.format;
	m_width = imageCreateInfo.extent.width;
	m_height = imageCreateInfo.extent.height;
	m_depth = imageCreateInfo.extent.depth;
	m_mipLevels = imageCreateInfo.mipLevels;
	m_arrayLayers = imageCreateInfo.arrayLayers;
	m_samples = imageCreateInfo.samples;
	m_tiling = imageCreateInfo.tiling;
	m_sharingMode = imageCreateInfo.sharingMode;
	
	VmaAllocationInfo allocInfo = {};
	if (vmaCreateImage(g_context.m_allocator, &imageCreateInfo, &allocCreateInfo, &m_image, &m_allocation, nullptr) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create image!", -1);
	}

	m_deviceMemory = allocInfo.deviceMemory;
	m_offset = allocInfo.offset;
	m_valid = true;
}

void VEngine::VKImage::destroy()
{
	if (m_valid)
	{
		vmaDestroyImage(g_context.m_allocator, m_image, m_allocation);

		m_valid = false;
	}
}

VkImage VEngine::VKImage::getImage() const
{
	return m_image;
}

VkImageType VEngine::VKImage::getImageType() const
{
	return m_imageType;
}

VkFormat VEngine::VKImage::getFormat() const
{
	return m_format;
}

uint32_t VEngine::VKImage::getWidth() const
{
	return m_width;
}

uint32_t VEngine::VKImage::getHeight() const
{
	return m_height;
}

uint32_t VEngine::VKImage::getDepth() const
{
	return m_depth;
}

uint32_t VEngine::VKImage::getMipLevels() const
{
	return m_mipLevels;
}

uint32_t VEngine::VKImage::getArrayLayers() const
{
	return m_arrayLayers;
}

VkSampleCountFlagBits VEngine::VKImage::getSamples() const
{
	return m_samples;
}

VkImageTiling VEngine::VKImage::getTiling() const
{
	return m_tiling;
}

VkSharingMode VEngine::VKImage::getSharingMode() const
{
	return m_sharingMode;
}

VmaAllocation VEngine::VKImage::getAllocation() const
{
	return m_allocation;
}

VkDeviceMemory VEngine::VKImage::getDeviceMemory() const
{
	return m_deviceMemory;
}

VkDeviceSize VEngine::VKImage::getOffset() const
{
	return m_offset;
}

bool VEngine::VKImage::isValid() const
{
	return m_valid;
}
