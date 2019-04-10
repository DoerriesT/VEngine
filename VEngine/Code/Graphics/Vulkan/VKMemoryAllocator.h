#pragma once
#include <vulkan/vulkan.h>
#include "Utility/ObjectPool.h"
#include "Utility/TLSFAllocator.h"

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

	struct VKMemoryBlockDebugInfo
	{
		uint32_t m_memoryType;
		size_t m_allocationSize;
		std::vector<TLSFSpanDebugInfo> m_spans;
	};

	class VKMemoryAllocator
	{
	public:
		VKMemoryAllocator();
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
		std::vector<VKMemoryBlockDebugInfo> getDebugInfo() const;
		size_t getMaximumBlockSize() const;
		void getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const;
		void destroy();

	private:
		enum 
		{
			MAX_BLOCK_SIZE = 256 * 1024 * 1024
		};

		class VKMemoryPool
		{
		public:
			void init(VkDevice device, uint32_t memoryType, VkDeviceSize bufferImageGranularity, VkDeviceSize preferredBlockSize);
			VkResult alloc(VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo);
			void free(VKAllocationInfo allocationInfo);
			VkResult mapMemory(size_t blockIndex, VkDeviceSize offset, void **data);
			void unmapMemory(size_t blockIndex);
			void getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const;
			void getDebugInfo(std::vector<VKMemoryBlockDebugInfo> &result) const;
			void destroy();

		private:
			enum
			{
				MAX_BLOCKS = 16
			};
			VkDevice m_device;
			uint32_t m_memoryType;
			VkDeviceSize m_bufferImageGranularity;
			VkDeviceSize m_preferredBlockSize;
			VkDeviceSize m_blockSizes[MAX_BLOCKS];
			VkDeviceMemory m_memory[MAX_BLOCKS];
			void *m_mappedPtr[MAX_BLOCKS];
			size_t m_mapCount[MAX_BLOCKS];
			TLSFAllocator *m_allocators[MAX_BLOCKS];

			bool allocFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, VKAllocationInfo &allocationInfo);
		};

		VkDevice m_device;
		VkPhysicalDeviceMemoryProperties m_memoryProperties;
		VkDeviceSize m_bufferImageGranularity;
		VkDeviceSize m_nonCoherentAtomSize;
		VkDeviceSize m_heapSizeLimits[VK_MAX_MEMORY_HEAPS];
		VKMemoryPool m_pools[VK_MAX_MEMORY_TYPES];
		DynamicObjectPool<VKAllocationInfo> m_allocationInfoPool;

		VkResult findMemoryTypeIndex(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex);
	};
}