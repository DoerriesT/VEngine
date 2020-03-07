#include "MappableBufferBlock.h"
#include <cassert>
#include "Utility/Utility.h"
#include "gal/GraphicsAbstractionLayer.h"

VEngine::MappableBufferBlock::MappableBufferBlock(gal::Buffer *buffer, uint64_t alignment)
	:m_currentOffset(),
	m_alignment(alignment),
	m_ptr(),
	m_buffer(buffer)
{
}

void VEngine::MappableBufferBlock::allocate(uint64_t &size, uint64_t &offset, gal::Buffer *&buffer, uint8_t *&data)
{
	assert(size);

	// offset after allocating from block
	uint64_t newOffset = Utility::alignUp(m_currentOffset + size, m_alignment);

	if (newOffset > m_buffer->getDescription().m_size)
	{
		// TODO find a better way
		assert(false);
	}
	else
	{
		size = newOffset - m_currentOffset;
		offset = m_currentOffset;
		buffer = m_buffer;
		data = m_ptr + m_currentOffset;
		m_currentOffset = newOffset;
	}
}

void VEngine::MappableBufferBlock::reset()
{
	m_currentOffset = 0;
}
