#pragma once
#include <vulkan\vulkan.h>
#include "vk_mem_alloc.h"

namespace VEngine
{
	struct VKImageData
	{
		VkImage m_image;
		VkImageView m_view;
		VkFormat m_format;
		VmaAllocation m_allocation;
	};
}