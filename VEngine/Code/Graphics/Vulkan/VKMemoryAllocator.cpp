#include "VKMemoryAllocator.h"
#include "Utility/Utility.h"
#include <cassert>

void VEngine::VKMemoryAllocator::VKMemoryPool::init(VkDevice device, uint32_t memoryType, VkDeviceSize bufferImageGranularity, VkDeviceSize preferredBlockSize)
{
	m_device = device;
	m_memoryType = memoryType;
	m_bufferImageGranularity = bufferImageGranularity;
	m_preferredBlockSize = preferredBlockSize;

	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_mappedPtr, 0, sizeof(m_mappedPtr));
	memset(m_mapCount, 0, sizeof(m_mapCount));
	memset(m_allocators, 0, sizeof(m_allocators));
}

VkResult VEngine::VKMemoryAllocator::VKMemoryPool::alloc(VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo)
{
	// search existing blocks and try to allocate
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (allocFromBlock(blockIndex, size, alignment, allocationInfo))
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
			if (!m_blockSizes[i])
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

		m_blockSizes[blockIndex] = memoryAllocateInfo.allocationSize;
		m_allocators[blockIndex] = new TLSFAllocator(static_cast<uint32_t>(memoryAllocateInfo.allocationSize), static_cast<uint32_t>(m_bufferImageGranularity));

		if (allocFromBlock(blockIndex, size, alignment, allocationInfo))
		{
			return VK_SUCCESS;
		}
		else
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}
}

void VEngine::VKMemoryAllocator::VKMemoryPool::free(VKAllocationInfo allocationInfo)
{
	m_allocators[allocationInfo.m_blockIndex]->free(allocationInfo.m_poolData);

	if (m_allocators[allocationInfo.m_blockIndex]->getAllocationCount() == 0)
	{
		vkFreeMemory(m_device, m_memory[allocationInfo.m_blockIndex], nullptr);
		m_blockSizes[allocationInfo.m_blockIndex] = 0;
		delete m_allocators[allocationInfo.m_blockIndex];
		m_allocators[allocationInfo.m_blockIndex] = nullptr;
	}
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

void VEngine::VKMemoryAllocator::VKMemoryPool::getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const
{
	free = 0;
	used = 0;
	wasted = 0;

	for (size_t i = 0; i < MAX_BLOCKS; ++i)
	{
		if (m_blockSizes[i])
		{
			uint32_t blockFree;
			uint32_t blockUsed;
			uint32_t blockWasted;

			m_allocators[i]->getFreeUsedWastedSizes(blockFree, blockUsed, blockWasted);

			free += blockFree;
			used += blockUsed;
			wasted += blockWasted;
		}
	}
}

void VEngine::VKMemoryAllocator::VKMemoryPool::getDebugInfo(std::vector<VKMemoryBlockDebugInfo>& result) const
{
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_blockSizes[blockIndex])
		{
			result.push_back({ m_memoryType, m_blockSizes[blockIndex] });
			std::vector<TLSFSpanDebugInfo> &spanInfos = result.back().m_spans;

			size_t count;
			m_allocators[blockIndex]->getDebugInfo(&count, nullptr);
			spanInfos.resize(count);
			m_allocators[blockIndex]->getDebugInfo(&count, spanInfos.data());
		}
	}
}

void VEngine::VKMemoryAllocator::VKMemoryPool::destroy()
{
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_blockSizes[blockIndex])
		{
			vkFreeMemory(m_device, m_memory[blockIndex], nullptr);

			delete m_allocators[blockIndex];
			m_allocators[blockIndex] = nullptr;
		}
	}

	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_mappedPtr, 0, sizeof(m_mappedPtr));
	memset(m_mapCount, 0, sizeof(m_mapCount));
	memset(m_allocators, 0, sizeof(m_allocators));
}

bool VEngine::VKMemoryAllocator::VKMemoryPool::allocFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo)
{
	uint32_t offset;
	if (m_blockSizes[blockIndex] > size &&  m_allocators[blockIndex]->alloc(static_cast<uint32_t>(size), static_cast<uint32_t>(alignment), offset, allocationInfo.m_poolData))
	{
		allocationInfo.m_memory = m_memory[blockIndex];
		allocationInfo.m_offset = offset;
		allocationInfo.m_size = size;
		allocationInfo.m_memoryType = m_memoryType;
		allocationInfo.m_poolIndex = m_memoryType;
		allocationInfo.m_blockIndex = blockIndex;

		return true;
	}
	return false;
}


void VEngine::VKMemoryAllocator::init(VkDevice device, VkPhysicalDevice physicalDevice)
{
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	m_device = device;
	m_bufferImageGranularity = deviceProperties.limits.bufferImageGranularity;
	m_nonCoherentAtomSize = deviceProperties.limits.nonCoherentAtomSize;
	m_usedMemorySize = 0;
	m_freeMemorySize = 0;
	m_wastedMemorySize = 0;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].init(m_device, i, m_bufferImageGranularity, MAX_BLOCK_SIZE);
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
	assert(bufferCreateInfo.size);
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

std::vector<VEngine::VKMemoryBlockDebugInfo> VEngine::VKMemoryAllocator::getDebugInfo() const
{
	std::vector<VKMemoryBlockDebugInfo> result;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].getDebugInfo(result);
	}
	return result;
}

size_t VEngine::VKMemoryAllocator::getMaximumBlockSize() const
{
	return MAX_BLOCK_SIZE;
}

void VEngine::VKMemoryAllocator::getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const
{
	free = 0;
	used = 0;
	wasted = 0;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		size_t poolFree = 0;
		size_t poolUsed = 0;
		size_t poolWasted = 0;

		m_pools[i].getFreeUsedWastedSizes(poolFree, poolUsed, poolWasted);

		free += poolFree;
		used += poolUsed;
		wasted += poolWasted;
	}
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
	int bestResultBitCount = -1;

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
			int bitCount = 0;
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
