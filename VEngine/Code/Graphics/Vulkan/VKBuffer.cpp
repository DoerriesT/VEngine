#include "VKBuffer.h"
#include "VKContext.h"
#include "Utility/Utility.h"

VEngine::VKBuffer::VKBuffer(const VkBufferCreateInfo &bufferCreateInfo, const VmaAllocationCreateInfo &allocCreateInfo)
	: m_size(bufferCreateInfo.size),
	m_sharingMode(bufferCreateInfo.sharingMode)
{
	if (vmaCreateBuffer(g_context.m_allocator, &bufferCreateInfo, &allocCreateInfo, &m_buffer, &m_allocation, &m_allocationInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create buffer!", -1);
	}
}

VEngine::VKBuffer::~VKBuffer()
{
	vmaDestroyBuffer(g_context.m_allocator, m_buffer, m_allocation);
}
