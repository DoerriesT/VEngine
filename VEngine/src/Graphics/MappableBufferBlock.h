#pragma once
#include <cstdint>
#include "gal/FwdDecl.h"

namespace VEngine
{
	class MappableBufferBlock
	{
	public:
		explicit MappableBufferBlock(gal::Buffer *buffer, uint64_t alignment);
		~MappableBufferBlock();
		void allocate(uint64_t &size, uint64_t &offset, gal::Buffer *&buffer, uint8_t *&data);
		void reset();

	private:
		uint64_t m_currentOffset;
		uint64_t m_alignment;
		uint8_t *m_ptr;
		gal::Buffer *m_buffer;
	};
}