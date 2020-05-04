#include "QueueDx12.h"
#include <vector>

void *VEngine::gal::QueueDx12::getNativeHandle() const
{
	return m_queue;
}

uint32_t VEngine::gal::QueueDx12::getFamilyIndex() const
{
	return m_queueFamily;
}

uint32_t VEngine::gal::QueueDx12::getTimestampValidBits() const
{
	return m_timestampValidBits;
}

void VEngine::gal::QueueDx12::submit(uint32_t count, const SubmitInfo *submitInfo)
{
	std::vector<ID3D12CommandList *> commandLists;

	for (size_t i = 0; i < count; ++i)
	{
		const auto &submit = submitInfo[i];

		for (size_t j = 0; j < submit.m_waitSemaphoreCount; ++j)
		{
			m_queue->Wait((ID3D12Fence *)submit.m_waitSemaphores[j]->getNativeHandle(), submit.m_waitValues[j]);
		}

		if (submit.m_commandListCount > 0)
		{
			commandLists.clear();
			commandLists.reserve(submit.m_commandListCount);

			for (size_t j = 0; j < submit.m_commandListCount; ++j)
			{
				commandLists.push_back((ID3D12CommandList *)submit.m_commandLists[j]->getNativeHandle());
			}

			m_queue->ExecuteCommandLists(submit.m_commandListCount, commandLists.data());
		}
		
		for (size_t j = 0; j < submit.m_signalSemaphoreCount; ++j)
		{
			m_queue->Signal((ID3D12Fence *)submit.m_signalSemaphores[j]->getNativeHandle(), submit.m_signalValues[j]);
		}
	}
}

void VEngine::gal::QueueDx12::waitIdle() const
{
	++m_semaphoreValue;
	m_queue->Signal((ID3D12Fence *)m_waitIdleSemaphore->getNativeHandle(), m_semaphoreValue);
	m_waitIdleSemaphore->wait(m_semaphoreValue);
}
