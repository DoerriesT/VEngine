#pragma once
#include <vulkan\vulkan.h>
#include "vk_mem_alloc.h"

namespace VEngine
{
	struct VKBufferData
	{
		VkBuffer m_buffer;
		VkBufferView m_view;
		VmaAllocation m_allocation;
		VmaAllocationInfo m_info;
	};
}