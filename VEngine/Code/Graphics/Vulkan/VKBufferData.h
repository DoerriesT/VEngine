#pragma once
#include <vulkan\vulkan.h>

namespace VEngine
{
	struct VKBufferData
	{
		VkBuffer m_buffer;
		VkDeviceMemory m_memory;
		VkDeviceSize m_size;
	};
}