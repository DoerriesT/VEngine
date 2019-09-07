#include "SyncPrimitivePool.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include <cassert>
#include <algorithm>
#include <set>

VEngine::SyncPrimitivePool::SyncPrimitivePool()
{
}

VEngine::SyncPrimitivePool::~SyncPrimitivePool()
{
	clear();
}

VkEvent VEngine::SyncPrimitivePool::acquireEvent()
{
	for (size_t i = m_eventBlocks.size(); i--;)
	{
		auto &block = m_eventBlocks[i];

		// this block has free items, use the first
		if (block.m_freeCount)
		{
			return block.m_items[--block.m_freeCount];
		}
	}

	// no block has free items, create a new block
	{
		constexpr size_t capacity = 256;
		Block<VkEvent> block{ new VkEvent[capacity], capacity, capacity };
		assert(block.m_items);
		VkEventCreateInfo createInfo{ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
		for (size_t i = 0; i < capacity; ++i)
		{
			if (vkCreateEvent(g_context.m_device, &createInfo, nullptr, &block.m_items[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create Event!", EXIT_FAILURE);
			}
		}
		m_eventBlocks.push_back(block);
	}
	auto &block = m_eventBlocks.back();
	return block.m_items[--block.m_freeCount];
}

VkSemaphore VEngine::SyncPrimitivePool::acquireSemaphore()
{
	for (size_t i = m_semaphoreBlocks.size(); i--;)
	{
		auto &block = m_semaphoreBlocks[i];

		// this block has free items, use the first
		if (block.m_freeCount)
		{
			return block.m_items[--block.m_freeCount];
		}
	}

	// no block has free items, create a new block
	{
		constexpr size_t capacity = 256;
		Block<VkSemaphore> block{ new VkSemaphore[capacity], capacity, capacity };
		assert(block.m_items);
		VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		for (size_t i = 0; i < capacity; ++i)
		{
			if (vkCreateSemaphore(g_context.m_device, &createInfo, nullptr, &block.m_items[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create Semaphore!", EXIT_FAILURE);
			}
		}
		m_semaphoreBlocks.push_back(block);
	}
	auto &block = m_semaphoreBlocks.back();
	return block.m_items[--block.m_freeCount];
}

VkFence VEngine::SyncPrimitivePool::acquireFence()
{
	for (size_t i = m_fenceBlocks.size(); i--;)
	{
		auto &block = m_fenceBlocks[i];

		// this block has free items, use the first
		if (block.m_freeCount)
		{
			return block.m_items[--block.m_freeCount];
		}
	}

	// no block has free items, create a new block
	{
		constexpr size_t capacity = 8;
		Block<VkFence> block{ new VkFence[capacity], capacity, capacity };
		assert(block.m_items);
		VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		for (size_t i = 0; i < capacity; ++i)
		{
			if (vkCreateFence(g_context.m_device, &createInfo, nullptr, &block.m_items[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create Fence!", EXIT_FAILURE);
			}
		}
		m_fenceBlocks.push_back(block);
	}
	auto &block = m_fenceBlocks.back();
	return block.m_items[--block.m_freeCount];
}

void VEngine::SyncPrimitivePool::freeEvent(VkEvent event)
{
	// search for a block to place this event into
	for (size_t i = m_eventBlocks.size(); i--;)
	{
		auto &block = m_eventBlocks[i];

		if (block.m_freeCount < block.m_capacity)
		{
			vkResetEvent(g_context.m_device, event);
			block.m_items[block.m_freeCount++] = event;
			return;
		}
	}
	// more events have been freed than allocated (double free?)
	assert(false);
}

void VEngine::SyncPrimitivePool::freeSemaphore(VkSemaphore semaphore)
{
	// search for a block to place this semaphore into
	for (size_t i = m_semaphoreBlocks.size(); i--;)
	{
		auto &block = m_semaphoreBlocks[i];

		if (block.m_freeCount < block.m_capacity)
		{
			block.m_items[block.m_freeCount++] = semaphore;
			return;
		}
	}
	// more semaphores have been freed than allocated (double free?)
	assert(false);
}

void VEngine::SyncPrimitivePool::freeFence(VkFence fence)
{
	// search for a block to place this fence into
	for (size_t i = m_fenceBlocks.size(); i--;)
	{
		auto &block = m_fenceBlocks[i];

		if (block.m_freeCount < block.m_capacity)
		{
			vkResetFences(g_context.m_device, 1, &fence);
			block.m_items[block.m_freeCount++] = fence;
			return;
		}
	}
	// more fences have been freed than allocated (double free?)
	assert(false);
}

void VEngine::SyncPrimitivePool::clearFreeBlocks()
{
	for (auto &block : m_eventBlocks)
	{
		if (block.m_capacity == block.m_freeCount)
		{
			for (size_t i = 0; i < block.m_capacity; ++i)
			{
				vkDestroyEvent(g_context.m_device, block.m_items[i], nullptr);
			}
		}
		delete[] block.m_items;
	}
	for (auto &block : m_semaphoreBlocks)
	{
		if (block.m_capacity == block.m_freeCount)
		{
			for (size_t i = 0; i < block.m_capacity; ++i)
			{
				vkDestroySemaphore(g_context.m_device, block.m_items[i], nullptr);
			}
		}
		delete[] block.m_items;
	}
	for (auto &block : m_fenceBlocks)
	{
		if (block.m_capacity == block.m_freeCount)
		{
			for (size_t i = 0; i < block.m_capacity; ++i)
			{
				vkDestroyFence(g_context.m_device, block.m_items[i], nullptr);
			}
		}
		delete[] block.m_items;
	}
	m_eventBlocks.erase(std::remove_if(m_eventBlocks.begin(), m_eventBlocks.end(), [](auto &block) {return block.m_capacity == block.m_freeCount; }), m_eventBlocks.end());
	m_semaphoreBlocks.erase(std::remove_if(m_semaphoreBlocks.begin(), m_semaphoreBlocks.end(), [](auto &block) {return block.m_capacity == block.m_freeCount; }), m_semaphoreBlocks.end());
	m_fenceBlocks.erase(std::remove_if(m_fenceBlocks.begin(), m_fenceBlocks.end(), [](auto &block) {return block.m_capacity == block.m_freeCount; }), m_fenceBlocks.end());
}

void VEngine::SyncPrimitivePool::clear()
{
	for (auto &block : m_eventBlocks)
	{
		for (size_t i = 0; i < block.m_capacity; ++i)
		{
			vkDestroyEvent(g_context.m_device, block.m_items[i], nullptr);
		}
		delete[] block.m_items;
	}
	for (auto &block : m_semaphoreBlocks)
	{
		for (size_t i = 0; i < block.m_capacity; ++i)
		{
			vkDestroySemaphore(g_context.m_device, block.m_items[i], nullptr);
		}
		delete[] block.m_items;
	}
	for (auto &block : m_fenceBlocks)
	{
		for (size_t i = 0; i < block.m_capacity; ++i)
		{
			vkDestroyFence(g_context.m_device, block.m_items[i], nullptr);
		}
		delete[] block.m_items;
	}
}

bool VEngine::SyncPrimitivePool::checkIntegrity()
{
	std::set<VkEvent> eventSet;
	size_t totalEvents = 0;
	for (auto &block : m_eventBlocks)
	{
		totalEvents += block.m_freeCount;
		eventSet.insert(block.m_items, block.m_items + block.m_freeCount);
	}

	std::set<VkSemaphore> semaphoreSet;
	size_t totalSemaphores = 0;
	for (auto &block : m_semaphoreBlocks)
	{
		totalSemaphores += block.m_freeCount;
		semaphoreSet.insert(block.m_items, block.m_items + block.m_freeCount);
	}

	std::set<VkFence> fenceSet;
	size_t totalFences = 0;
	for (auto &block : m_fenceBlocks)
	{
		totalFences += block.m_freeCount;
		fenceSet.insert(block.m_items, block.m_items + block.m_freeCount);
	}

	return eventSet.size() == totalEvents && semaphoreSet.size() == totalSemaphores && fenceSet.size() == totalFences;
}
