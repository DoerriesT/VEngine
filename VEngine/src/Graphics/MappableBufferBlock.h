#pragma once
#include <cstdint>
#include "gal/FwdDecl.h"

namespace VEngine
{
	class MappableBufferBlock
	{
	public:
		explicit MappableBufferBlock(gal::Buffer *buffer);
		~MappableBufferBlock();
		void allocate(uint64_t alignment, uint64_t &size, uint64_t &offset, gal::Buffer *&buffer, uint8_t *&data);
		void reset();

	private:
		uint64_t m_currentOffset;
		uint8_t *m_ptr;
		gal::Buffer *m_buffer;
	};
}