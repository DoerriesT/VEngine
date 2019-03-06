#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "VKMemoryAllocator.h"

namespace VEngine
{
	class VKBuffer
	{
	public:
		void create(const VkBufferCreateInfo &bufferCreateInfo, const VKAllocationCreateInfo &allocCreateInfo);
		void destroy();
		VkBuffer getBuffer() const;
		VkDeviceSize getSize() const;
		VkSharingMode getSharingMode() const;
		VKAllocationHandle getAllocation() const;
		VkDeviceMemory getDeviceMemory() const;
		VkDeviceSize getOffset() const;
		bool isValid() const;

	private:
		VkBuffer m_buffer;
		VkDeviceSize m_size;
		VkSharingMode m_sharingMode;
		VKAllocationHandle m_allocation;
		VkDeviceMemory m_deviceMemory;
		VkDeviceSize m_offset;
		bool m_valid;
	};
}