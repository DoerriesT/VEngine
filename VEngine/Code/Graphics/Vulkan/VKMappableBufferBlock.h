#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include "VKBuffer.h"

namespace VEngine
{
	class VKMappableBufferBlock
	{
	public:
		explicit VKMappableBufferBlock(VKBuffer &buffer, VkDeviceSize alignment);
		void allocate(VkDeviceSize &size, VkDeviceSize &offset, VkBuffer &buffer, uint8_t *&data);
		void reset();

	private:
		VkDeviceSize m_currentOffset;
		VkDeviceSize m_alignment;
		uint8_t *m_ptr;
		VKBuffer &m_buffer;
	};
}