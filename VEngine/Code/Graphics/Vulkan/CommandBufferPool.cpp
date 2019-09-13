#include "CommandBufferPool.h"
#include "VKContext.h"
#include "Utility/Utility.h"

VEngine::CommandBufferPool::CommandBufferPool(uint32_t allocationChunkSize)
	:m_allocationChunkSize(allocationChunkSize)
{
}

VEngine::CommandBufferPool::~CommandBufferPool()
{
	reset();
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandPools[i] != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(g_context.m_device, m_commandPools[i], nullptr);
		}
	}
}

VkCommandBuffer VEngine::CommandBufferPool::acquire(uint32_t queueFamilyIndex)
{
	const uint32_t poolIndex = queueFamilyIndex == g_context.m_queueFamilyIndices.m_graphicsFamily ? 0 : g_context.m_queueFamilyIndices.m_computeFamily ? 1 : 2;

	if (m_commandPools[poolIndex] == VK_NULL_HANDLE)
	{
		VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex };
		if (vkCreateCommandPool(g_context.m_device, &poolCreateInfo, nullptr, &m_commandPools[poolIndex]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create command pool!", EXIT_FAILURE);
		}
	}

	const size_t currentPoolSize = m_commandBuffers[poolIndex].size();
	if (m_nextFreeCmdBuf[poolIndex] == currentPoolSize)
	{
		m_commandBuffers[poolIndex].resize(currentPoolSize + m_allocationChunkSize);

		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_commandPools[poolIndex], VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_allocationChunkSize };
		if (vkAllocateCommandBuffers(g_context.m_device, &allocInfo, m_commandBuffers[poolIndex].data() + currentPoolSize) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate command buffers!", EXIT_FAILURE);
		}
	}

	return m_commandBuffers[poolIndex][m_nextFreeCmdBuf[poolIndex]++];
}

void VEngine::CommandBufferPool::reset()
{
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandPools[i] != VK_NULL_HANDLE)
		{
			vkResetCommandPool(g_context.m_device, m_commandPools[i], 0);
		}
		m_nextFreeCmdBuf[i] = 0;
	}
}
