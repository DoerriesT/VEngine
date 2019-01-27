#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	class VKResourceManager
	{
	public:
		VkSemaphore acquireSemaphore();
		VkEvent acquireEvent();
		void freeSemaphore(VkSemaphore semaphore);
		void freeEvent(VkEvent event);

	private:
		std::vector<VkSemaphore> m_freeSemaphores;
		std::vector<VkEvent> m_freeEvents;
		std::vector<VkSemaphore> m_usedSemaphores;
		std::vector<VkEvent> m_usedEvents;
	};
}