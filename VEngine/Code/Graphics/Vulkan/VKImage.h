#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc_include.h"

namespace VEngine
{
	struct VKImage
	{
		VkImage m_image;
		VkImageType m_imageType;
		VkFormat m_format;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_depth;
		uint32_t m_mipLevels;
		uint32_t m_arrayLayers;
		VkSampleCountFlagBits m_samples;
		VkImageTiling m_tiling;
		VkSharingMode m_sharingMode;
		VmaAllocation m_allocation;
		VmaAllocationInfo m_allocationInfo;

		explicit VKImage(const VkImageCreateInfo &imageCreateInfo, const VmaAllocationCreateInfo &allocCreateInfo);
		VKImage(const VKImage &) = delete;
		VKImage(const VKImage &&) = delete;
		VKImage &operator= (const VKImage &) = delete;
		VKImage &operator= (const VKImage &&) = delete;
		~VKImage();
	};
}