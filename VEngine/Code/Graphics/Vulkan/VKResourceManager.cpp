#include "VKResourceManager.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "Utility/ContainerUtility.h"
#include <cassert>

VkSemaphore VEngine::VKResourceManager::acquireSemaphore()
{
	if (m_freeSemaphores.empty())
	{
		VkSemaphore semaphore;

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(g_context.m_device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create semaphores!", -1);
		}

		m_usedSemaphores.push_back(semaphore);

		return semaphore;
	}
	else
	{
		VkSemaphore semaphore = m_freeSemaphores.back();

		ContainerUtility::quickRemove(m_freeSemaphores, semaphore);

		m_usedSemaphores.push_back(semaphore);

		return semaphore;
	}
}

VkEvent VEngine::VKResourceManager::acquireEvent()
{
	if (m_freeEvents.empty())
	{
		VkEvent event;

		VkEventCreateInfo createInfo = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };

		if (vkCreateEvent(g_context.m_device, &createInfo, nullptr, &event) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create event!", -1);
		}

		m_usedEvents.push_back(event);

		return event;
	}
	else
	{
		VkEvent event = m_freeEvents.back();

		ContainerUtility::quickRemove(m_freeEvents, event);

		m_usedEvents.push_back(event);

		return event;
	}
}

void VEngine::VKResourceManager::freeSemaphore(VkSemaphore semaphore)
{
	assert(ContainerUtility::contains(m_usedSemaphores, semaphore));
	ContainerUtility::quickRemove(m_usedSemaphores, semaphore);
	m_freeSemaphores.push_back(semaphore);
}

void VEngine::VKResourceManager::freeEvent(VkEvent event)
{
	assert(ContainerUtility::contains(m_usedEvents, event));
	ContainerUtility::quickRemove(m_usedEvents, event);
	m_freeEvents.push_back(event);
}
