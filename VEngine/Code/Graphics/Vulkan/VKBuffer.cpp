#include "VKBuffer.h"
#include "VKContext.h"
#include "Utility/Utility.h"

void VEngine::VKBuffer::create(const VkBufferCreateInfo &bufferCreateInfo, const VKAllocationCreateInfo &allocCreateInfo)
{
	m_size = bufferCreateInfo.size;
	m_sharingMode = bufferCreateInfo.sharingMode;

	if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_buffer, m_allocation) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create buffer!", -1);
	}

	VKAllocationInfo allocInfo = g_context.m_allocator.getAllocationInfo(m_allocation);

	m_deviceMemory = allocInfo.m_memory;
	m_offset = allocInfo.m_offset;
	m_valid = true;
}

void VEngine::VKBuffer::destroy()
{
	if (m_valid)
	{
		g_context.m_allocator.destroyBuffer(m_buffer, m_allocation);

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

VEngine::VKAllocationHandle VEngine::VKBuffer::getAllocation() const
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
