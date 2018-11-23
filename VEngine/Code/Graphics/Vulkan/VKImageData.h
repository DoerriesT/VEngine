#pragma once
#include <vulkan\vulkan.h>

namespace VEngine
{
	struct VKImageData
	{
		VkImage m_image;
		VkImageView m_view;
		VkFormat m_format;
		VkDeviceMemory m_memory;
	};
}