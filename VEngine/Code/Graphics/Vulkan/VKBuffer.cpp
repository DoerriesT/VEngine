#include "VKBuffer.h"
#include "VKContext.h"
#include "Utility/Utility.h"

void VEngine::VKBuffer::create(const VkBufferCreateInfo & bufferCreateInfo, const VmaAllocationCreateInfo & allocCreateInfo)
{
	m_size = bufferCreateInfo.size;
	m_sharingMode = bufferCreateInfo.sharingMode;

	VmaAllocationInfo allocInfo = {};
	if (vmaCreateBuffer(g_context.m_allocator, &bufferCreateInfo, &allocCreateInfo, &m_buffer, &m_allocation, &allocInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create buffer!", -1);
	}

	m_deviceMemory = allocInfo.deviceMemory;
	m_offset = allocInfo.offset;
	m_valid = true;
}

void VEngine::VKBuffer::destroy()
{
	if (m_valid)
	{
		vmaDestroyBuffer(g_context.m_allocator, m_buffer, m_allocation);

		m_valid = false;
	}
}

VkBuffer VEngine::VKBuffer::getBuffer() const
{
	return m_buffer;
}

VkDeviceSize VEngine::VKBuffer::getSize() const
{
	return m_size;
}

VkSharingMode VEngine::VKBuffer::getSharingMode() const
{
	return m_sharingMode;
}

VmaAllocation VEngine::VKBuffer::getAllocation() const
{
	return m_allocation;
}

VkDeviceMemory VEngine::VKBuffer::getDeviceMemory() const
{
	return m_deviceMemory;
}

VkDeviceSize VEngine::VKBuffer::getOffset() const
{
	return m_offset;
}

bool VEngine::VKBuffer::isValid() const
{
	return m_valid;
}
