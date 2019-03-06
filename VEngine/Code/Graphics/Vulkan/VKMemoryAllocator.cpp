#include "VKMemoryAllocator.h"
#include "Utility/Utility.h"
#include <cassert>
#include <iostream>

size_t spanCount = 0;

void VEngine::VKMemoryAllocator::VKMemoryPool::init(VkDevice device, uint32_t memoryType, VkDeviceSize pageSize, VkDeviceSize splitSizeThreshold, VkDeviceSize preferredBlockSize)
{
	m_device = device;
	m_memoryType = memoryType;
	m_pageSize = pageSize;
	m_preferredBlockSize = preferredBlockSize;
	m_splitSizeThreshold = splitSizeThreshold;
	m_preferredBlockSize = preferredBlockSize;

	m_allocatedBlocks = 0;
	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_firstLevelBitsets, 0, sizeof(m_firstLevelBitsets));
	memset(m_secondLevelBitsets, 0, sizeof(m_secondLevelBitsets));
	memset(m_freeSpans, 0, sizeof(m_freeSpans));
	memset(m_firstPhysicalSpan, 0, sizeof(m_firstPhysicalSpan));
	memset(m_allocationCounts, 0, sizeof(m_allocationCounts));
	memset(m_mappedPtr, 0, sizeof(m_mappedPtr));
	memset(m_mapCount, 0, sizeof(m_mapCount));
}

VkResult VEngine::VKMemoryAllocator::VKMemoryPool::alloc(VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo)
{
	// search existing blocks and try to allocate
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_allocatedBlocks[blockIndex]
			&& m_firstLevelBitsets[blockIndex]
			&& allocateFromBlock(blockIndex, size, alignment, allocationInfo) == VK_SUCCESS)
		{
			return VK_SUCCESS;
		}
	}

	// cant allocate from any existing block, create a new one
	{
		// search for index of new block
		size_t blockIndex = ~size_t(0);
		for (size_t i = 0; i < MAX_BLOCKS; ++i)
		{
			if (!m_allocatedBlocks[i])
			{
				blockIndex = i;
				break;
			}
		}

		// maximum number of blocks allocated
		if (blockIndex >= MAX_BLOCKS)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memoryAllocateInfo.allocationSize = m_preferredBlockSize;
		memoryAllocateInfo.memoryTypeIndex = m_memoryType;

		if (vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_memory[blockIndex]) != VK_SUCCESS)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		m_allocatedBlocks[blockIndex] = true;
		m_blockSizes[blockIndex] = memoryAllocateInfo.allocationSize;

		// add span of memory, spanning the whole block
		Span *span = m_spanPool.alloc();
		*span =
		{
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			0,
			memoryAllocateInfo.allocationSize
		};

		++spanCount;

		size_t firstLevelIndex = 0;
		size_t secondLevelIndex = 0;
		mappingInsert(span->m_size, blockIndex, firstLevelIndex, secondLevelIndex);

		m_firstPhysicalSpan[blockIndex] = span;
		m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex] = span;

		// update bitsets
		m_secondLevelBitsets[blockIndex][firstLevelIndex] |= 1 << secondLevelIndex;
		m_firstLevelBitsets[blockIndex] |= 1 << firstLevelIndex;

		// try to allocate from new block
		return allocateFromBlock(blockIndex, size, alignment, allocationInfo);
	}
}

void VEngine::VKMemoryAllocator::VKMemoryPool::free(VKAllocationInfo allocationInfo)
{
	Span *span = reinterpret_cast<Span *>(allocationInfo.m_poolData);
	assert(!span->m_next);
	assert(!span->m_previous);

	// is next physical span also free? -> merge
	{
		Span *nextPhysical = span->m_nextPhysical;
		if (nextPhysical && !nextPhysical->m_used)
		{
			// remove next physical span from free list
			size_t firstLevelIndex = 0;
			size_t secondLevelIndex = 0;
			mappingInsert(nextPhysical->m_size, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);
			removeSpanFromFreeList(nextPhysical, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);

			// merge spans
			span->m_size += nextPhysical->m_size;
			span->m_nextPhysical = nextPhysical->m_nextPhysical;
			if (nextPhysical->m_nextPhysical)
			{
				nextPhysical->m_nextPhysical->m_previousPhysical = span;
			}

			m_spanPool.free(nextPhysical);

			assert(spanCount);
			--spanCount;
		}
	}

	// is previous physical span also free? -> merge
	{
		Span *previousPhysical = span->m_previousPhysical;
		if (previousPhysical && !previousPhysical->m_used)
		{
			// remove previous physical span from free list
			size_t firstLevelIndex = 0;
			size_t secondLevelIndex = 0;
			mappingInsert(previousPhysical->m_size, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);
			removeSpanFromFreeList(previousPhysical, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);

			// merge spans
			previousPhysical->m_size += span->m_size;
			previousPhysical->m_nextPhysical = span->m_nextPhysical;
			if (span->m_nextPhysical)
			{
				span->m_nextPhysical->m_previousPhysical = previousPhysical;
			}

			m_spanPool.free(span);

			span = previousPhysical;

			assert(spanCount);
			--spanCount;
		}
	}

	// add span to free list
	{
		size_t firstLevelIndex = 0;
		size_t secondLevelIndex = 0;
		mappingInsert(span->m_size, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(span, allocationInfo.m_blockIndex, firstLevelIndex, secondLevelIndex);
	}

	--m_allocationCounts[allocationInfo.m_blockIndex];

	if (m_allocationCounts[allocationInfo.m_blockIndex] == 0)
	{
		vkFreeMemory(m_device, m_memory[allocationInfo.m_blockIndex], nullptr);
		m_allocatedBlocks[allocationInfo.m_blockIndex] = false;
		m_blockSizes[allocationInfo.m_blockIndex] = 0;
	}

	//std::cout << "f" << spanCount << std::endl;
}

VkResult VEngine::VKMemoryAllocator::VKMemoryPool::mapMemory(size_t blockIndex, VkDeviceSize offset, void **data)
{
	VkResult result = VK_SUCCESS;
	if (m_mapCount[blockIndex] == 0)
	{
		result = vkMapMemory(m_device, m_memory[blockIndex], 0, VK_WHOLE_SIZE, 0, &m_mappedPtr[blockIndex]);
	}

	if (result == VK_SUCCESS)
	{
		++m_mapCount[blockIndex];
		*data = (char *)m_mappedPtr[blockIndex] + offset;
	}

	return result;
}

void VEngine::VKMemoryAllocator::VKMemoryPool::unmapMemory(size_t blockIndex)
{
	assert(m_mapCount[blockIndex]);
	--m_mapCount[blockIndex];

	if (m_mapCount[blockIndex] == 0)
	{
		vkUnmapMemory(m_device, m_memory[blockIndex]);
		m_mappedPtr[blockIndex] = nullptr;
	}
}

void VEngine::VKMemoryAllocator::VKMemoryPool::destroy()
{
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_allocatedBlocks[blockIndex])
		{
			vkFreeMemory(m_device, m_memory[blockIndex], nullptr);

			Span *span = m_firstPhysicalSpan[blockIndex];
			while (span)
			{
				Span *next = span->m_nextPhysical;
				m_spanPool.free(span);
				span = next;
			}
		}
	}

	m_allocatedBlocks = 0;
	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_firstLevelBitsets, 0, sizeof(m_firstLevelBitsets));
	memset(m_secondLevelBitsets, 0, sizeof(m_secondLevelBitsets));
	memset(m_freeSpans, 0, sizeof(m_freeSpans));
	memset(m_firstPhysicalSpan, 0, sizeof(m_firstPhysicalSpan));
	memset(m_allocationCounts, 0, sizeof(m_allocationCounts));
}

void VEngine::VKMemoryAllocator::VKMemoryPool::mappingInsert(VkDeviceSize size, size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex)
{
	firstLevelIndex = Utility::findLastSetBit(size);
	secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVEL_INDEX)) - MAX_SECOND_LEVEL_INDEX;
	size_t i = size_t(log2(size));
	size_t j = size_t((size - (1 << i)) / (1 << (i - MAX_LOG2_SECOND_LEVEL_INDEX)));
	assert(firstLevelIndex == i);
	assert(secondLevelIndex == j);
}

void VEngine::VKMemoryAllocator::VKMemoryPool::mappingSearch(VkDeviceSize size, size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex)
{
	size_t t = (1 << (Utility::findLastSetBit(size) - MAX_LOG2_SECOND_LEVEL_INDEX)) - 1;
	size += t;
	firstLevelIndex = Utility::findLastSetBit(size);
	secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVEL_INDEX)) - MAX_SECOND_LEVEL_INDEX;
	size &= ~t;
}

bool VEngine::VKMemoryAllocator::VKMemoryPool::findFreeSpan(size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex)
{
	uint32_t bitsetTmp = m_secondLevelBitsets[blockIndex][firstLevelIndex] & (~0 << secondLevelIndex);

	if (bitsetTmp)
	{
		secondLevelIndex = Utility::findFirstSetBit(bitsetTmp);
		return true;
	}
	else
	{
		firstLevelIndex = Utility::findFirstSetBit(m_firstLevelBitsets[blockIndex] & (~0 << (firstLevelIndex + 1)));
		if (firstLevelIndex != (uint32_t(0) - 1))
		{
			secondLevelIndex = Utility::findFirstSetBit(m_secondLevelBitsets[blockIndex][firstLevelIndex]);
			return true;
		}
	}
	return false;
}

VkResult VEngine::VKMemoryAllocator::VKMemoryPool::allocateFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo)
{
	assert(m_allocatedBlocks[blockIndex]);
	assert(m_firstLevelBitsets[blockIndex]);

	size_t firstLevelIndex = 0;
	size_t secondLevelIndex = 0;
	size = (size + 1) / 2 * 2;
	// add some margin to accound for alignment
	VkDeviceSize requestedSize = size + alignment;
	requestedSize -= alignment & 1;

	// rounds up requested size to next power of two and finds indices of a list containing spans of the requested size class
	mappingSearch(requestedSize, blockIndex, firstLevelIndex, secondLevelIndex);

	// finds a free span and updates indices to the indices of the actual free list of the span
	if (!findFreeSpan(blockIndex, firstLevelIndex, secondLevelIndex))
	{
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}

	Span *freeSpan = m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex];

	assert(freeSpan);
	assert(freeSpan->m_size >= size);
	assert(!freeSpan->m_previous);

	const VkDeviceSize alignedOffset = (freeSpan->m_offset + alignment - 1) / alignment * alignment;
	assert(alignedOffset + size <= freeSpan->m_offset + freeSpan->m_size);

	removeSpanFromFreeList(freeSpan, blockIndex, firstLevelIndex, secondLevelIndex);

	const VkDeviceSize beginMargin = alignedOffset - freeSpan->m_offset;
	const VkDeviceSize endMargin = freeSpan->m_offset + freeSpan->m_size - alignedOffset - size;

	// split span at beginning?
	if (beginMargin >= m_splitSizeThreshold)
	{
		Span *beginSpan = m_spanPool.alloc();
		*beginSpan =
		{
			nullptr,
			nullptr,
			freeSpan->m_previousPhysical,
			freeSpan,
			freeSpan->m_offset,
			beginMargin
		};

		++spanCount;

		if (freeSpan->m_previousPhysical)
		{
			assert(freeSpan->m_previousPhysical->m_nextPhysical == freeSpan);
			freeSpan->m_previousPhysical->m_nextPhysical = beginSpan;
		}
		else
		{
			m_firstPhysicalSpan[blockIndex] = beginSpan;
		}

		freeSpan->m_offset += beginMargin;
		freeSpan->m_size -= beginMargin;
		freeSpan->m_previousPhysical = beginSpan;

		// add begin span to free list
		mappingInsert(beginSpan->m_size, blockIndex, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(beginSpan, blockIndex, firstLevelIndex, secondLevelIndex);
	}

	// split span at end?
	if (endMargin >= m_splitSizeThreshold)
	{
		Span *endSpan = m_spanPool.alloc();
		*endSpan =
		{
			nullptr,
			nullptr,
			freeSpan,
			freeSpan->m_nextPhysical,
			freeSpan->m_offset + size,
			endMargin
		};

		++spanCount;

		if (freeSpan->m_nextPhysical)
		{
			assert(freeSpan->m_nextPhysical->m_previousPhysical == freeSpan);
			freeSpan->m_nextPhysical->m_previousPhysical = endSpan;
		}

		freeSpan->m_nextPhysical = endSpan;
		freeSpan->m_size -= endMargin;

		// add end span to free list
		mappingInsert(endSpan->m_size, blockIndex, firstLevelIndex, secondLevelIndex);
		addSpanToFreeList(endSpan, blockIndex, firstLevelIndex, secondLevelIndex);
	}

	++m_allocationCounts[blockIndex];

	// fill out allocation info
	allocationInfo.m_memory = m_memory[blockIndex];
	allocationInfo.m_offset = alignedOffset;
	allocationInfo.m_size = size;
	allocationInfo.m_memoryType = m_memoryType;
	allocationInfo.m_poolIndex = m_memoryType;
	allocationInfo.m_blockIndex = blockIndex;
	allocationInfo.m_poolData = freeSpan;

	//std::cout << "a" << spanCount << std::endl;

	return VK_SUCCESS;
}

void VEngine::VKMemoryAllocator::VKMemoryPool::addSpanToFreeList(Span *span, size_t blockIndex, size_t firstLevelIndex, size_t secondLevelIndex)
{
	// set span as new head
	Span *head = m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex];
	m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex] = span;

	// link span and previous head and mark span as free
	span->m_previous = nullptr;
	span->m_next = head;
	span->m_used = false;
	if (head)
	{
		assert(!head->m_previous);
		head->m_previous = span;
	}

	// update bitsets
	m_secondLevelBitsets[blockIndex][firstLevelIndex] |= 1 << secondLevelIndex;
	m_firstLevelBitsets[blockIndex] |= 1 << firstLevelIndex;
}

void VEngine::VKMemoryAllocator::VKMemoryPool::removeSpanFromFreeList(Span *span, size_t blockIndex, size_t firstLevelIndex, size_t secondLevelIndex)
{
	// is span head of list?
	if (!span->m_previous)
	{
		assert(span == m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex]);

		m_freeSpans[blockIndex][firstLevelIndex][secondLevelIndex] = span->m_next;
		if (span->m_next)
		{
			span->m_next->m_previous = nullptr;
		}
		else
		{
			// update bitsets, since list is now empty
			m_secondLevelBitsets[blockIndex][firstLevelIndex] &= ~(1 << secondLevelIndex);
			if (m_secondLevelBitsets[blockIndex][firstLevelIndex] == 0)
			{
				m_firstLevelBitsets[blockIndex] &= ~(1 << firstLevelIndex);
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
	span->m_used = true;
}

void VEngine::VKMemoryAllocator::init(VkDevice device, VkPhysicalDevice physicalDevice)
{
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	m_device = device;
	m_bufferImageGranularity = deviceProperties.limits.bufferImageGranularity;
	m_nonCoherentAtomSize = deviceProperties.limits.nonCoherentAtomSize;
	m_pageSize = m_bufferImageGranularity < 16 ? 16 : m_bufferImageGranularity;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].init(m_device, i, m_pageSize, m_pageSize, 256 * 1024 * 1024);
	}
}

VkResult VEngine::VKMemoryAllocator::alloc(const VKAllocationCreateInfo &allocationCreateInfo, const VkMemoryRequirements &memoryRequirements, VKAllocationHandle &allocationHandle)
{
	uint32_t memoryTypeIndex;
	if (findMemoryTypeIndex(memoryRequirements.memoryTypeBits, allocationCreateInfo.m_requiredFlags, allocationCreateInfo.m_preferredFlags, memoryTypeIndex) != VK_SUCCESS)
	{
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	size_t allocationInfoIndex = ~size_t(0);
	if (!m_freeAllocationInfos.empty())
	{
		allocationInfoIndex = m_freeAllocationInfos.back();
		m_freeAllocationInfos.pop_back();
	}
	else
	{
		allocationInfoIndex = m_allocationInfos.size();
		m_allocationInfos.push_back({});
	}

	VKAllocationInfo &allocationInfo = m_allocationInfos[allocationInfoIndex];

	VkResult result = m_pools[memoryTypeIndex].alloc(memoryRequirements.size, memoryRequirements.alignment, allocationInfo);

	if (result == VK_SUCCESS)
	{
		allocationHandle = VKAllocationHandle(allocationInfoIndex + 1);
	}

	return result;
}

VkResult VEngine::VKMemoryAllocator::createImage(const VKAllocationCreateInfo &allocationCreateInfo, const VkImageCreateInfo &imageCreateInfo, VkImage &image, VKAllocationHandle &allocationHandle)
{
	VkResult result = vkCreateImage(m_device, &imageCreateInfo, nullptr, &image);

	if (result != VK_SUCCESS)
	{
		return result;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(m_device, image, &memoryRequirements);

	result = alloc(allocationCreateInfo, memoryRequirements, allocationHandle);

	if (result != VK_SUCCESS)
	{
		vkDestroyImage(m_device, image, nullptr);
		return result;
	}

	VKAllocationInfo &allocationInfo = m_allocationInfos[(size_t)allocationHandle - 1];

	result = vkBindImageMemory(m_device, image, allocationInfo.m_memory, allocationInfo.m_offset);

	if (result != VK_SUCCESS)
	{
		vkDestroyImage(m_device, image, nullptr);
		free(allocationHandle);
		return result;
	}

	return VK_SUCCESS;
}

VkResult VEngine::VKMemoryAllocator::createBuffer(const VKAllocationCreateInfo &allocationCreateInfo, const VkBufferCreateInfo &bufferCreateInfo, VkBuffer &buffer, VKAllocationHandle &allocationHandle)
{
	VkResult result = vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &buffer);

	if (result != VK_SUCCESS)
	{
		return result;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

	result = alloc(allocationCreateInfo, memoryRequirements, allocationHandle);

	if (result != VK_SUCCESS)
	{
		vkDestroyBuffer(m_device, buffer, nullptr);
		return result;
	}

	VKAllocationInfo &allocationInfo = m_allocationInfos[(size_t)allocationHandle - 1];

	result = vkBindBufferMemory(m_device, buffer, allocationInfo.m_memory, allocationInfo.m_offset);

	if (result != VK_SUCCESS)
	{
		vkDestroyBuffer(m_device, buffer, nullptr);
		free(allocationHandle);
		return result;
	}

	return VK_SUCCESS;
}

void VEngine::VKMemoryAllocator::free(VKAllocationHandle allocationHandle)
{
	size_t allocationInfoIndex = (size_t)allocationHandle - 1;
	VKAllocationInfo &allocationInfo = m_allocationInfos[allocationInfoIndex];
	m_pools[allocationInfo.m_poolIndex].free(allocationInfo);
	memset(&allocationInfo, 0, sizeof(allocationInfo));
	m_freeAllocationInfos.push_back(allocationInfoIndex);
}

void VEngine::VKMemoryAllocator::destroyImage(VkImage image, VKAllocationHandle allocationHandle)
{
	vkDestroyImage(m_device, image, nullptr);
	free(allocationHandle);
}

void VEngine::VKMemoryAllocator::destroyBuffer(VkBuffer buffer, VKAllocationHandle allocationHandle)
{
	vkDestroyBuffer(m_device, buffer, nullptr);
	free(allocationHandle);
}

VkResult VEngine::VKMemoryAllocator::mapMemory(VKAllocationHandle allocationHandle, void **data)
{
	VKAllocationInfo &allocationInfo = m_allocationInfos[(size_t)allocationHandle - 1];

	assert(m_memoryProperties.memoryTypes[allocationInfo.m_memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	return m_pools[allocationInfo.m_poolIndex].mapMemory(allocationInfo.m_blockIndex, allocationInfo.m_offset, data);
}

void VEngine::VKMemoryAllocator::unmapMemory(VKAllocationHandle allocationHandle)
{
	VKAllocationInfo &allocationInfo = m_allocationInfos[(size_t)allocationHandle - 1];

	assert(m_memoryProperties.memoryTypes[allocationInfo.m_memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	m_pools[allocationInfo.m_poolIndex].unmapMemory(allocationInfo.m_blockIndex);
}

VEngine::VKAllocationInfo VEngine::VKMemoryAllocator::getAllocationInfo(VKAllocationHandle allocationHandle)
{
	return m_allocationInfos[(size_t)allocationHandle - 1];
}

void VEngine::VKMemoryAllocator::destroy()
{
	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].destroy();
	}
}

VkResult VEngine::VKMemoryAllocator::findMemoryTypeIndex(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex)
{
	memoryTypeIndex = ~uint32_t(0);
	uint32_t bestResultBitCount = 0;

	for (uint32_t memoryIndex = 0; memoryIndex < m_memoryProperties.memoryTypeCount; ++memoryIndex)
	{
		const bool isRequiredMemoryType = memoryTypeBitsRequirement & (1 << memoryIndex);

		const VkMemoryPropertyFlags properties = m_memoryProperties.memoryTypes[memoryIndex].propertyFlags;
		const bool hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

		if (isRequiredMemoryType && hasRequiredProperties)
		{
			uint32_t presentBits = properties & preferredProperties;

			// all preferred bits are present
			if (presentBits == preferredProperties)
			{
				memoryTypeIndex = memoryIndex;
				return VK_SUCCESS;
			}

			// only some bits are present -> count them
			uint32_t bitCount = 0;
			for (uint32_t bit = 0; bit < 32; ++bit)
			{
				bitCount += (presentBits & (1 << bit)) >> bit;
			}

			// save memory type with highest bit count of present preferred flags
			if (bitCount > bestResultBitCount)
			{
				bestResultBitCount = bitCount;
				memoryTypeIndex = memoryIndex;
			}
		}
	}

	return memoryTypeIndex != ~uint32_t(0) ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
}
