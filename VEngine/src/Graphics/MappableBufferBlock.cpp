#include "MappableBufferBlock.h"
#include <cassert>
#include "Utility/Utility.h"
#include "gal/GraphicsAbstractionLayer.h"

VEngine::MappableBufferBlock::MappableBufferBlock(gal::Buffer *buffer)
	:m_currentOffset(),
	m_ptr(),
	m_buffer(buffer)
{
	m_buffer->map((void **)&m_ptr);
}

VEngine::MappableBufferBlock::~MappableBufferBlock()
{
	m_buffer->unmap();
}

void VEngine::MappableBufferBlock::allocate(uint64_t alignment, uint64_t &size, uint64_t &offset, gal::Buffer *&buffer, uint8_t *&data)
{
	assert(size);

	offset = Utility::alignUp(m_currentOffset, alignment);
	
	// offset after allocating from block
	uint64_t newOffset = Utility::alignUp(offset + size, alignment);

	if (newOffset > m_buffer->getDescription().m_size)
	{
		assert(false);
	}
	else
	{
		size = newOffset - offset;
		buffer = m_buffer;
		data = m_ptr + offset;
		m_currentOffset = newOffset;
	}
}

void VEngine::MappableBufferBlock::reset()
{
	m_currentOffset = 0;
}
