#include "VKMappableBufferBlock.h"
#include "Utility/Utility.h"
#include "VKContext.h"

VEngine::VKMappableBufferBlock::VKMappableBufferBlock(VKBuffer &buffer, VkDeviceSize alignment)
	:m_currentOffset(),
	m_alignment(alignment),
	m_buffer(buffer)
{
	g_context.m_allocator.mapMemory(m_buffer.getAllocation(), (void **)&m_ptr);
}

void VEngine::VKMappableBufferBlock::allocate(VkDeviceSize &size, VkDeviceSize &offset, VkBuffer &buffer, uint8_t *&data)
{
	assert(size);

	// offset after allocating from block
	VkDeviceSize newOffset = Utility::alignUp(m_currentOffset + size, m_alignment);
	
	if (newOffset > m_buffer.getSize())
	{
		// TODO find a better way
		assert(false);
	}
	else
	{
		size = newOffset - m_currentOffset;
		offset = m_currentOffset;
		buffer = m_buffer.getBuffer();
		data = m_ptr + m_currentOffset;
		m_currentOffset = newOffset;
	}
}

void VEngine::VKMappableBufferBlock::reset()
{
	m_currentOffset = 0;
}
