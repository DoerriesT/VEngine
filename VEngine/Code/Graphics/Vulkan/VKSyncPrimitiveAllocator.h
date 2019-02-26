#pragma once
#include <vulkan/vulkan.h>
#include <bitset>

namespace VEngine
{
	class VKSyncPrimitiveAllocator
	{
	public:
		explicit VKSyncPrimitiveAllocator();
		VKSyncPrimitiveAllocator(const VKSyncPrimitiveAllocator &) = delete;
		VKSyncPrimitiveAllocator(const VKSyncPrimitiveAllocator &&) = delete;
		VKSyncPrimitiveAllocator &operator= (const VKSyncPrimitiveAllocator &) = delete;
		VKSyncPrimitiveAllocator &operator= (const VKSyncPrimitiveAllocator &&) = delete;
		~VKSyncPrimitiveAllocator();
		VkEvent acquireEvent();
		VkSemaphore acquireSemaphore();
		VkFence acquireFence();
		void freeEvent(VkEvent event);
		void freeSemaphore(VkSemaphore semaphore);
		void freeFence(VkFence fence);

	private:
		enum
		{
			EVENTS = 512, 
			SEMAPHORES = 256, 
			FENCES = 8
		};
		std::bitset<EVENTS> m_freeEvents;
		std::bitset<SEMAPHORES> m_freeSemaphores;
		std::bitset<FENCES> m_freeFences;
		VkEvent m_events[EVENTS];
		VkSemaphore m_semaphores[SEMAPHORES];
		VkFence m_fences[FENCES];
	};
}