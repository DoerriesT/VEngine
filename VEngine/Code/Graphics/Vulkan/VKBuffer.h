#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc_include.h"

namespace VEngine
{
	class VKBuffer
	{
	public:
		void create(const VkBufferCreateInfo &bufferCreateInfo, const VmaAllocationCreateInfo &allocCreateInfo);
		void destroy();
		VkBuffer getBuffer() const;
		VkDeviceSize getSize() const;
		VkSharingMode getSharingMode() const;
		VmaAllocation getAllocation() const;
		VkDeviceMemory getDeviceMemory() const;
		VkDeviceSize getOffset() const;
		bool isValid() const;

	private:
		VkBuffer m_buffer;
		VkDeviceSize m_size;
		VkSharingMode m_sharingMode;
		VmaAllocation m_allocation;
		VkDeviceMemory m_deviceMemory;
		VkDeviceSize m_offset;
		bool m_valid;
	};
}