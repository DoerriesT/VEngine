#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc_include.h"

namespace VEngine
{
	struct VKBuffer
	{
		VkBuffer m_buffer;
		VkDeviceSize m_size;
		VkSharingMode m_sharingMode;
		VmaAllocation m_allocation;
		VmaAllocationInfo m_allocationInfo;

		explicit VKBuffer(const VkBufferCreateInfo &bufferCreateInfo, const VmaAllocationCreateInfo &allocCreateInfo);
		VKBuffer(const VKBuffer &) = delete;
		VKBuffer(const VKBuffer &&) = delete;
		VKBuffer &operator= (const VKBuffer &) = delete;
		VKBuffer &operator= (const VKBuffer &&) = delete;
		~VKBuffer();
	};
}