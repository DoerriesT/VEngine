#pragma once
#include "Graphics/Vulkan/volk.h"
#include <vector>

namespace VEngine
{
	class SyncPrimitivePool
	{
	public:
		SyncPrimitivePool();
		SyncPrimitivePool(const SyncPrimitivePool &) = delete;
		SyncPrimitivePool(const SyncPrimitivePool &&) = delete;
		SyncPrimitivePool &operator= (const SyncPrimitivePool &) = delete;
		SyncPrimitivePool &operator= (const SyncPrimitivePool &&) = delete;
		~SyncPrimitivePool();
		VkEvent acquireEvent();
		VkSemaphore acquireSemaphore();
		VkFence acquireFence();
		void freeEvent(VkEvent event);
		void freeSemaphore(VkSemaphore semaphore);
		void freeFence(VkFence fence);
		void clearFreeBlocks();
		void clear();
		bool checkIntegrity();

	private:
		template<typename T>
		struct Block
		{
			T *m_items;
			size_t m_freeCount;
			size_t m_capacity;
		};

		std::vector<Block<VkEvent>> m_eventBlocks;
		std::vector<Block<VkSemaphore>> m_semaphoreBlocks;
		std::vector<Block<VkFence>> m_fenceBlocks;
	};
}