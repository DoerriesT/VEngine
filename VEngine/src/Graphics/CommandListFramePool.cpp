#include "CommandListFramePool.h"
#include "gal/GraphicsAbstractionLayer.h"

using namespace VEngine::gal;

VEngine::CommandListFramePool::CommandListFramePool(GraphicsDevice *graphicsDevice, uint32_t allocationChunkSize)
	:m_graphicsDevice(graphicsDevice),
	m_allocationChunkSize(allocationChunkSize),
	m_commandListPools(),
	m_commandLists(),
	m_nextFreeCmdList()
{
}

VEngine::CommandListFramePool::~CommandListFramePool()
{
	reset();
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandListPools[i])
		{
			m_graphicsDevice->destroyCommandListPool(m_commandListPools[i]);
		}
	}
}

VEngine::gal::CommandList *VEngine::CommandListFramePool::acquire(Queue *queue)
{
	const uint32_t poolIndex = queue == m_graphicsDevice->getGraphicsQueue() ? 0 : queue == m_graphicsDevice->getComputeQueue() ? 1 : 2;

	if (!m_commandListPools[poolIndex])
	{
		m_graphicsDevice->createCommandListPool(queue, &m_commandListPools[poolIndex]);
	}

	const size_t currentPoolSize = m_commandLists[poolIndex].size();
	if (m_nextFreeCmdList[poolIndex] == currentPoolSize)
	{
		m_commandLists[poolIndex].resize(currentPoolSize + m_allocationChunkSize);

		m_commandListPools[poolIndex]->allocate(m_allocationChunkSize, m_commandLists[poolIndex].data() + currentPoolSize);
	}

	return m_commandLists[poolIndex][m_nextFreeCmdList[poolIndex]++];
}

void VEngine::CommandListFramePool::reset()
{
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandListPools[i])
		{
			m_commandListPools[i]->reset();
		}
		m_nextFreeCmdList[i] = 0;
	}
}
