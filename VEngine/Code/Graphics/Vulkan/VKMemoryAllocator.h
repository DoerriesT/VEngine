#pragma once
#include <vulkan/vulkan.h>
#include <cmath>
#include <bitset>
#include <vector>
#include "Utility/ObjectPool.h"

namespace VEngine
{
	typedef struct VKAllocationHandle_t *VKAllocationHandle;

	struct VKAllocationCreateInfo
	{
		VkMemoryPropertyFlags m_requiredFlags;
		VkMemoryPropertyFlags m_preferredFlags;
	};

	struct VKAllocationInfo
	{
		VkDeviceMemory m_memory;
		VkDeviceSize m_offset;
		VkDeviceSize m_size;
		uint32_t m_memoryType;
		size_t m_poolIndex;
		size_t m_blockIndex;
		void *m_poolData;
	};

	struct VKMemorySpanDebugInfo
	{
		enum class State
		{
			FREE,
			USED,
			WASTED
		};
		size_t m_offset;
		size_t m_size;
		State m_state;
	};

	struct VKMemoryBlockDebugInfo
	{
		uint32_t m_memoryType;
		size_t m_allocationSize;
		std::vector<VKMemorySpanDebugInfo> m_spans;
	};

	class VKMemoryAllocator
	{
	public:
		void init(VkDevice device, VkPhysicalDevice physicalDevice);
		VkResult alloc(const VKAllocationCreateInfo &allocationCreateInfo, const VkMemoryRequirements &memoryRequirements, VKAllocationHandle &allocationHandle);
		VkResult createImage(const VKAllocationCreateInfo &allocationCreateInfo, const VkImageCreateInfo &imageCreateInfo, VkImage &image, VKAllocationHandle &allocationHandle);
		VkResult createBuffer(const VKAllocationCreateInfo &allocationCreateInfo, const VkBufferCreateInfo &bufferCreateInfo, VkBuffer &buffer, VKAllocationHandle &allocationHandle);
		void free(VKAllocationHandle allocationHandle);
		void destroyImage(VkImage image, VKAllocationHandle allocationHandle);
		void destroyBuffer(VkBuffer buffer, VKAllocationHandle allocationHandle);
		VkResult mapMemory(VKAllocationHandle allocationHandle, void **data);
		void unmapMemory(VKAllocationHandle allocationHandle);
		VKAllocationInfo getAllocationInfo(VKAllocationHandle allocationHandle);
		std::vector<VKMemoryBlockDebugInfo> getDebugInfo();
		size_t getMaximumBlockSize();
		size_t getFreeMemorySize();
		size_t getUsedMemorySize();
		size_t getWastedMemorySize();
		void destroy();

	private:
		enum 
		{
			MAX_BLOCK_SIZE = 256 * 1024 * 1024
		};

		class VKMemoryPool
		{
		public:
			void init(VkDevice device, uint32_t memoryType, VkDeviceSize bufferImageGranularity, VkDeviceSize splitSizeThreshold, VkDeviceSize preferredBlockSize,
				VkDeviceSize *usedMemorySize,VkDeviceSize *freeMemorySize,VkDeviceSize *wastedMemorySize);
			VkResult alloc(VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo);
			void free(VKAllocationInfo allocationInfo);
			VkResult mapMemory(size_t blockIndex, VkDeviceSize offset, void **data);
			void unmapMemory(size_t blockIndex);
			void getDebugInfo(std::vector<VKMemoryBlockDebugInfo> &result);
			void destroy();

		private:
			enum
			{
				MAX_BLOCKS = 16,
				MAX_FIRST_LEVEL_INDEX = 28,
				MAX_LOG2_SECOND_LEVEL_INDEX = 5,
				MAX_SECOND_LEVEL_INDEX = 1 << MAX_LOG2_SECOND_LEVEL_INDEX,
			};

			struct Span
			{
				Span *m_previous;
				Span *m_next;
				Span *m_previousPhysical;
				Span *m_nextPhysical;
				VkDeviceSize m_offset;
				VkDeviceSize m_size;
				VkDeviceSize m_usedOffset;
				VkDeviceSize m_usedSize;
			};

			VkDevice m_device;
			uint32_t m_memoryType;
			std::bitset<MAX_BLOCKS> m_allocatedBlocks;
			VkDeviceSize m_bufferImageGranularity;
			VkDeviceSize m_splitSizeThreshold;
			VkDeviceSize m_preferredBlockSize;
			VkDeviceSize m_blockSizes[MAX_BLOCKS];
			VkDeviceMemory m_memory[MAX_BLOCKS];
			uint32_t m_firstLevelBitsets[MAX_BLOCKS];
			uint32_t m_secondLevelBitsets[MAX_BLOCKS][MAX_FIRST_LEVEL_INDEX];
			Span *m_freeSpans[MAX_BLOCKS][MAX_FIRST_LEVEL_INDEX][MAX_SECOND_LEVEL_INDEX];
			Span *m_firstPhysicalSpan[MAX_BLOCKS];
			size_t m_allocationCounts[MAX_BLOCKS];
			void *m_mappedPtr[MAX_BLOCKS];
			size_t m_mapCount[MAX_BLOCKS];
			VkDeviceSize *m_usedMemorySize;
			VkDeviceSize *m_freeMemorySize;
			VkDeviceSize *m_wastedMemorySize;
			StaticObjectPool<Span, 256> m_spanPool;

			// returns indices of the list holding memory blocks that lie in the same size class as requestedSize.
			// used for inserting free blocks into the data structure
			void mappingInsert(VkDeviceSize size, size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex);
			// returns indices of the list holding memory blocks that are one size class above requestedSize,
			// ensuring, that any block in the list can be used. used for alloc
			void mappingSearch(VkDeviceSize size, size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex);
			bool findFreeSpan(size_t blockIndex, size_t &firstLevelIndex, size_t &secondLevelIndex);
			VkResult allocateFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo);
			void addSpanToFreeList(Span *span, size_t blockIndex, size_t firstLevelIndex, size_t secondLevelIndex);
			void removeSpanFromFreeList(Span *span, size_t blockIndex, size_t firstLevelIndex, size_t secondLevelIndex);
		};

		VkDevice m_device;
		VkPhysicalDeviceMemoryProperties m_memoryProperties;
		VkDeviceSize m_bufferImageGranularity;
		VkDeviceSize m_nonCoherentAtomSize;
		VkDeviceSize m_pageSize;
		VkDeviceSize m_heapSizeLimits[VK_MAX_MEMORY_HEAPS];
		VKMemoryPool m_pools[VK_MAX_MEMORY_TYPES];
		VkDeviceSize m_usedMemorySize;
		VkDeviceSize m_freeMemorySize;
		VkDeviceSize m_wastedMemorySize;
		std::vector<VKAllocationInfo> m_allocationInfos;
		std::vector<size_t> m_freeAllocationInfos;

		VkResult findMemoryTypeIndex(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex);
	};
}