#include "VKSyncPrimitiveAllocator.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include <cassert>

VEngine::VKSyncPrimitiveAllocator::VKSyncPrimitiveAllocator()
{
	m_freeEvents.flip();
	m_freeSemaphores.flip();
	m_freeFences.flip();

	VkEventCreateInfo eventCreateInfo = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

	for (size_t i = 0; i < EVENTS; ++i)
	{
		if (vkCreateEvent(g_context.m_device, &eventCreateInfo, nullptr, &m_events[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create Event!", -1);
		}
	}

	for (size_t i = 0; i < SEMAPHORES; ++i)
	{
		if (vkCreateSemaphore(g_context.m_device, &semaphoreCreateInfo, nullptr, &m_semaphores[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create Semaphore!", -1);
		}
	}

	for (size_t i = 0; i < FENCES; ++i)
	{
		if (vkCreateFence(g_context.m_device, &fenceCreateInfo, nullptr, &m_fences[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create Fence!", -1);
		}
	}
}

VEngine::VKSyncPrimitiveAllocator::~VKSyncPrimitiveAllocator()
{
	//assert(m_freeEvents.all());
	//assert(m_freeSemaphores.all());
	//assert(m_freeFences.all());

	for (size_t i = 0; i < EVENTS; ++i)
	{
		vkDestroyEvent(g_context.m_device, m_events[i], nullptr);
	}

	for (size_t i = 0; i < SEMAPHORES; ++i)
	{
		vkDestroySemaphore(g_context.m_device, m_semaphores[i], nullptr);
	}

	for (size_t i = 0; i < FENCES; ++i)
	{
		vkDestroyFence(g_context.m_device, m_fences[i], nullptr);
	}
}

VkEvent VEngine::VKSyncPrimitiveAllocator::acquireEvent()
{
	for (size_t i = 0; i < EVENTS; ++i)
	{
		if (m_freeEvents[i])
		{
			m_freeEvents[i] = false;
			return m_events[i];
		}
	}

	Utility::fatalExit("Out of VkEvents!", -1);
	return nullptr;
}

VkSemaphore VEngine::VKSyncPrimitiveAllocator::acquireSemaphore()
{
	for (size_t i = 0; i < SEMAPHORES; ++i)
	{
		if (m_freeSemaphores[i])
		{
			m_freeSemaphores[i] = false;
			return m_semaphores[i];
		}
	}

	Utility::fatalExit("Out of VkSemaphores!", -1);
	return nullptr;
}

VkFence VEngine::VKSyncPrimitiveAllocator::acquireFence()
{
	for (size_t i = 0; i < FENCES; ++i)
	{
		if (m_freeFences[i])
		{
			m_freeFences[i] = false;
			return m_fences[i];
		}
	}

	Utility::fatalExit("Out of VkSemaphores!", -1);
	return nullptr;
}

void VEngine::VKSyncPrimitiveAllocator::freeEvent(VkEvent event)
{
	for (size_t i = 0; i < EVENTS; ++i)
	{
		if (m_events[i] == event)
		{
			vkResetEvent(g_context.m_device, event);
			m_freeEvents[i] = true;
			break;
		}
	}
}

void VEngine::VKSyncPrimitiveAllocator::freeSemaphore(VkSemaphore semaphore)
{
	for (size_t i = 0; i < SEMAPHORES; ++i)
	{
		if (m_semaphores[i] == semaphore)
		{
			m_freeSemaphores[i] = true;
			break;
		}
	}
}

void VEngine::VKSyncPrimitiveAllocator::freeFence(VkFence fence)
{
	for (size_t i = 0; i < FENCES; ++i)
	{
		if (m_fences[i] == fence)
		{
			vkResetFences(g_context.m_device, 1, &fence);
			m_freeFences[i] = true;
			break;
		}
	}
}
