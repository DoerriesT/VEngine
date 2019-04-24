#include "TLSFAllocator.h"
#include <memory>
#include <cassert>
#include "Utility.h"

VEngine::TLSFAllocator::TLSFAllocator(uint32_t memorySize, uint32_t pageSize)
	:m_memorySize(memorySize),
	m_pageSize(pageSize),
	m_firstLevelBitset(),
	m_firstPhysicalSpan(nullptr),
	m_allocationCount(),
	m_freeSize(memorySize),
	m_usedSize(),
	m_spanPool(256)
{
	memset(m_secondLevelBitsets, 0, sizeof(m_secondLevelBitsets));
	memset(m_freeSpans, 0, sizeof(m_freeSpans));

	// add span of memory, spanning the whole block
	Span *span = m_spanPool.alloc();
	memset(span, 0, sizeof(Span));

	span->m_size = m_memorySize;

	uint32_t firstLevelIndex = 0;
	uint32_t secondLevelIndex = 0;
	mappingInsert(m_memorySize, firstLevelIndex, secondLevelIndex);

	m_firstPhysicalSpan = span;

	addSpanToFreeList(span, firstLevelIndex, secondLevelIndex);
}

bool VEngine::TLSFAllocator::alloc(uint32_t size, uint32_t alignment, uint32_t &spanOffset, void *&backingSpan)
{
	assert(size >= 32);
	uint32_t firstLevelIndex = 0;
	uint32_t secondLevelIndex = 0;

	// add some margin to accound for alignment
	uint32_t requestedSize = size + alignment;

	// size must be larger-equal than MAX_SECOND_LEVELS
	requestedSize = requestedSize < MAX_SECOND_LEVELS ? MAX_SECOND_LEVELS : requestedSize;

	// rounds up requested size to next power of two and finds indices of a list containing spans of the requested size class
	mappingSearch(requestedSize, firstLevelIndex, secondLevelIndex);

	// finds a free span and updates indices to the indices of the actual free list of the span
	if (!findFreeSpan(firstLevelIndex, secondLevelIndex))
	{
		return false;
	}

	Span *freeSpan = m_freeSpans[firstLevelIndex][secondLevelIndex];

	assert(freeSpan);
	assert(freeSpan->m_size >= size);
	assert(!freeSpan->m_previous);
	assert(!freeSpan->m_previousPhysical || freeSpan->m_previousPhysical->m_offset + freeSpan->m_previousPhysical->m_size == freeSpan->m_offset);
	assert(!freeSpan->m_nextPhysical || freeSpan->m_offset + freeSpan->m_size == freeSpan->m_nextPhysical->m_offset);

	removeSpanFromFreeList(freeSpan, firstLevelIndex, secondLevelIndex);

	// offset into this span where alignment requirement is satisfied
	const uint32_t alignedOffset = Utility::alignUp(freeSpan->m_offset, alignment);
	assert(alignedOffset + size <= freeSpan->m_offset + freeSpan->m_size);

	uint32_t nextLowerPageSizeOffset = Utility::alignDown(alignedOffset, m_pageSize);
	assert(nextLowerPageSizeOffset <= alignedOffset);
	assert(nextLowerPageSizeOffset >= freeSpan->m_offset);

	uint32_t nextUpperPageSizeOffset = Utility::alignUp(alignedOffset + size, m_pageSize);
	assert(nextUpperPageSizeOffset >= alignedOffset + size);
	assert(nextUpperPageSizeOffset <= freeSpan->m_offset + freeSpan->m_size);

	// all spans must start at a multiple of page size, so calculate margin from next lower multiple, not from actual used offset
	const uint32_t beginMargin = nextLowerPageSizeOffset - freeSpan->m_offset;
	// since all spans start at a multiple of page size, they must also end at such a multiple, so calculate margin from next upper
	// multiple to end of span
	const uint32_t endMargin = freeSpan->m_offset + freeSpan->m_size - nextUpperPageSizeOffset;

	// split span at beginning?
	if (beginMargin >= m_pageSize)
	{
		Span *beginSpan = m_spanPool.alloc();
		memset(beginSpan, 0, sizeof(Span));

		beginSpan->m_previousPhysical = freeSpan->m_previousPhysical;
		beginSpan->m_nextPhysical = freeSpan;
		beginSpan->m_offset = freeSpan->m_offset;
		beginSpan->m_size = beginMargin;

		if (freeSpan->m_previousPhysical)
		{
			assert(freeSpan->m_previousPhysical->m_nextPhysical == freeSpan);
			freeSpan->m_previousPhysical->m_nextPhysical = beginSpan;
		}
		else
		{
			m_firstPhysicalSpan = beginSpan;
		}

		freeSpan->m_offset += beginMargin;
		freeSpan->m_size -= beginMargin;
		freeSpan->m_previousPhysical = beginSpan;

		// add begin span to free list
		mappingInsert(beginSpan->m_size, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(beginSpan, firstLevelIndex, secondLevelIndex);
	}

	// split span at end?
	if (endMargin >= m_pageSize)
	{
		Span *endSpan = m_spanPool.alloc();
		memset(endSpan, 0, sizeof(Span));

		endSpan->m_previousPhysical = freeSpan;
		endSpan->m_nextPhysical = freeSpan->m_nextPhysical;
		endSpan->m_offset = nextUpperPageSizeOffset;
		endSpan->m_size = endMargin;

		if (freeSpan->m_nextPhysical)
		{
			assert(freeSpan->m_nextPhysical->m_previousPhysical == freeSpan);
			freeSpan->m_nextPhysical->m_previousPhysical = endSpan;
		}

		freeSpan->m_nextPhysical = endSpan;
		freeSpan->m_size -= endMargin;

		// add end span to free list
		mappingInsert(endSpan->m_size, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(endSpan, firstLevelIndex, secondLevelIndex);
	}

	++m_allocationCount;

	// fill out allocation info
	spanOffset = alignedOffset;
	backingSpan = freeSpan;

	// update span usage
	freeSpan->m_usedOffset = alignedOffset;
	freeSpan->m_usedSize = size;

	m_freeSize -= freeSpan->m_size;
	m_usedSize += freeSpan->m_usedSize;

#ifdef _DEBUG
	checkIntegrity();
#endif // _DEBUG

	return true;
}

void VEngine::TLSFAllocator::free(void *backingSpan)
{
	Span *span = reinterpret_cast<Span *>(backingSpan);
	assert(!span->m_next);
	assert(!span->m_previous);

	m_freeSize += span->m_size;
	m_usedSize -= span->m_usedSize;

	// is next physical span also free? -> merge
	{
		Span *nextPhysical = span->m_nextPhysical;
		if (nextPhysical && nextPhysical->m_usedSize == 0)
		{
			// remove next physical span from free list
			uint32_t firstLevelIndex = 0;
			uint32_t secondLevelIndex = 0;
			mappingInsert(nextPhysical->m_size, firstLevelIndex, secondLevelIndex);
			removeSpanFromFreeList(nextPhysical, firstLevelIndex, secondLevelIndex);

			// merge spans
			span->m_size += nextPhysical->m_size;
			span->m_nextPhysical = nextPhysical->m_nextPhysical;
			if (nextPhysical->m_nextPhysical)
			{
				nextPhysical->m_nextPhysical->m_previousPhysical = span;
			}

			m_spanPool.free(nextPhysical);
		}
	}

	// is previous physical span also free? -> merge
	{
		Span *previousPhysical = span->m_previousPhysical;
		if (previousPhysical && previousPhysical->m_usedSize == 0)
		{
			// remove previous physical span from free list
			uint32_t firstLevelIndex = 0;
			uint32_t secondLevelIndex = 0;
			mappingInsert(previousPhysical->m_size, firstLevelIndex, secondLevelIndex);
			removeSpanFromFreeList(previousPhysical, firstLevelIndex, secondLevelIndex);

			// merge spans
			previousPhysical->m_size += span->m_size;
			previousPhysical->m_nextPhysical = span->m_nextPhysical;
			if (span->m_nextPhysical)
			{
				span->m_nextPhysical->m_previousPhysical = previousPhysical;
			}

			m_spanPool.free(span);

			span = previousPhysical;
		}
	}

	// add span to free list
	{
		uint32_t firstLevelIndex = 0;
		uint32_t secondLevelIndex = 0;
		mappingInsert(span->m_size, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(span, firstLevelIndex, secondLevelIndex);
	}

	--m_allocationCount;

#ifdef _DEBUG
	checkIntegrity();
#endif // _DEBUG
}

uint32_t VEngine::TLSFAllocator::getAllocationCount() const
{
	return m_allocationCount;
}

void VEngine::TLSFAllocator::getDebugInfo(size_t *count, TLSFSpanDebugInfo *info) const
{
	if (!info)
	{
		*count = m_spanPool.getAllocationCount();
	}
	else
	{
		Span *span = m_firstPhysicalSpan;
		size_t i = 0;
		while(span && i < *count)
		{
			// span is free
			if (span->m_usedSize == 0 && i < *count)
			{
				TLSFSpanDebugInfo &spanInfo = info[i++];
				spanInfo.m_offset = span->m_offset;
				spanInfo.m_size = span->m_size;
				spanInfo.m_state = TLSFSpanDebugInfo::State::FREE;
			}
			else
			{
				// wasted size at start of suballocation
				if (span->m_usedOffset > span->m_offset && i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_offset;
					spanInfo.m_size = span->m_usedOffset - span->m_offset;
					spanInfo.m_state = TLSFSpanDebugInfo::State::WASTED;
				}

				// used space of suballocation
				if (i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_usedOffset;
					spanInfo.m_size = span->m_usedSize;
					spanInfo.m_state = TLSFSpanDebugInfo::State::USED;
				}

				// wasted space at end of suballocation
				if (span->m_offset + span->m_size > span->m_usedOffset + span->m_usedSize && i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_usedOffset + span->m_usedSize;
					spanInfo.m_size = span->m_offset + span->m_size - span->m_usedOffset - span->m_usedSize;
					spanInfo.m_state = TLSFSpanDebugInfo::State::WASTED;
				}
			}

			span = span->m_nextPhysical;
		}
	}
}

uint32_t VEngine::TLSFAllocator::getMemorySize() const
{
	return m_memorySize;
}

uint32_t VEngine::TLSFAllocator::getPageSize() const
{
	return m_pageSize;
}

void VEngine::TLSFAllocator::getFreeUsedWastedSizes(uint32_t &free, uint32_t &used, uint32_t &wasted) const
{
	free = m_freeSize;
	used = m_usedSize;
	wasted = m_memorySize - m_freeSize - m_usedSize;
}

void VEngine::TLSFAllocator::mappingInsert(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	if (size < SMALL_BLOCK)
	{
		firstLevelIndex = 0;
		secondLevelIndex = size / (SMALL_BLOCK / MAX_SECOND_LEVELS);
	}
	else
	{
		firstLevelIndex = Utility::findLastSetBit(size);
		secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVELS)) - MAX_SECOND_LEVELS;
	}

	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);
}

void VEngine::TLSFAllocator::mappingSearch(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	if (size < SMALL_BLOCK)
	{
		firstLevelIndex = 0;
		secondLevelIndex = size / (SMALL_BLOCK / MAX_SECOND_LEVELS);
	}
	else
	{
		uint32_t t = (1 << (Utility::findLastSetBit(size) - MAX_LOG2_SECOND_LEVELS)) - 1;
		size += t;
		firstLevelIndex = Utility::findLastSetBit(size);
		secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVELS)) - MAX_SECOND_LEVELS;
		size &= ~t;
	}

	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);
}

bool VEngine::TLSFAllocator::findFreeSpan(uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);

	uint32_t bitsetTmp = m_secondLevelBitsets[firstLevelIndex] & (~0u << secondLevelIndex);

	if (bitsetTmp)
	{
		secondLevelIndex = Utility::findFirstSetBit(bitsetTmp);
		return true;
	}
	else
	{
		firstLevelIndex = Utility::findFirstSetBit(m_firstLevelBitset & (~0u << (firstLevelIndex + 1)));
		if (firstLevelIndex != (uint32_t(0) - 1))
		{
			secondLevelIndex = Utility::findFirstSetBit(m_secondLevelBitsets[firstLevelIndex]);
			return true;
		}
	}
	return false;
}

void VEngine::TLSFAllocator::addSpanToFreeList(Span *span, uint32_t firstLevelIndex, uint32_t secondLevelIndex)
{
	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);

	// set span as new head
	Span *head = m_freeSpans[firstLevelIndex][secondLevelIndex];
	m_freeSpans[firstLevelIndex][secondLevelIndex] = span;

	// link span and previous head and mark span as free
	span->m_previous = nullptr;
	span->m_next = head;
	span->m_usedOffset = 0;
	span->m_usedSize = 0;
	if (head)
	{
		assert(!head->m_previous);
		head->m_previous = span;
	}

	// update bitsets
	m_secondLevelBitsets[firstLevelIndex] |= 1 << secondLevelIndex;
	m_firstLevelBitset |= 1 << firstLevelIndex;
}

void VEngine::TLSFAllocator::removeSpanFromFreeList(Span *span, uint32_t firstLevelIndex, uint32_t secondLevelIndex)
{
	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);

	// is span head of list?
	if (!span->m_previous)
	{
		assert(span == m_freeSpans[firstLevelIndex][secondLevelIndex]);

		m_freeSpans[firstLevelIndex][secondLevelIndex] = span->m_next;
		if (span->m_next)
		{
			span->m_next->m_previous = nullptr;
		}
		else
		{
			// update bitsets, since list is now empty
			m_secondLevelBitsets[firstLevelIndex] &= ~(1 << secondLevelIndex);
			if (m_secondLevelBitsets[firstLevelIndex] == 0)
			{
				m_firstLevelBitset &= ~(1 << firstLevelIndex);
			}
		}
	}
	else
	{
		span->m_previous->m_next = span->m_next;
		if (span->m_next)
		{
			span->m_next->m_previous = span->m_previous;
		}
	}

	// remove previous and next pointers in span and mark it as used
	span->m_next = nullptr;
	span->m_previous = nullptr;
	span->m_usedOffset = span->m_offset;
	span->m_usedSize = span->m_size;
}

void VEngine::TLSFAllocator::checkIntegrity()
{
	Span *span = m_firstPhysicalSpan;
	uint32_t currentOffset = 0;
	while (span)
	{
		assert(span->m_offset == currentOffset);
		currentOffset += span->m_size;
		span = span->m_nextPhysical;
	}
	assert(currentOffset == m_memorySize);
}
