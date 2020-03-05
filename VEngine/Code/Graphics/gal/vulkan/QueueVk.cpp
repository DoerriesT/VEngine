#include "QueueVk.h"
#include <vector>
#include "Graphics/gal/Common.h"
#include "Graphics/gal/Semaphore.h"
#include "Graphics/gal/CommandList.h"
#include "UtilityVk.h"

void *VEngine::gal::QueueVk::getNativeHandle() const
{
	return m_queue;
}

uint32_t VEngine::gal::QueueVk::getFamilyIndex() const
{
	return m_queueFamily;
}

void VEngine::gal::QueueVk::submit(uint32_t count, const SubmitInfo *submitInfo)
{
	size_t semaphoreCount = 0;
	size_t commandBufferCount = 0;
	for (uint32_t i = 0; i < count; ++i)
	{
		semaphoreCount += submitInfo[i].m_waitSemaphoreCount + submitInfo[i].m_signalSemaphoreCount;
		commandBufferCount += submitInfo[i].m_commandListCount;
	}

	// TODO: avoid dynamic heap allocation
	std::vector<VkSubmitInfo> submitInfoVk(count);
	std::vector<VkTimelineSemaphoreSubmitInfo> timelineSemaphoreInfoVk(count);
	std::vector<VkSemaphore> semaphores;
	semaphores.reserve(semaphoreCount);
	std::vector<VkCommandBuffer> commandBuffers;
	commandBuffers.resize(commandBufferCount);

	for (uint32_t i = 0; i < count; ++i)
	{
		size_t waitSemaphoreOffset = semaphores.size();
		size_t commandBuffersOffset = commandBuffers.size();

		for (uint32_t j = 0; j < submitInfo[i].m_waitSemaphoreCount; ++j)
		{
			semaphores.push_back((VkSemaphore)submitInfo[i].m_waitSemaphores[j]->getNativeHandle());
			commandBuffers.push_back((VkCommandBuffer)submitInfo[i].m_commandLists[j]->getNativeHandle());
		}

		size_t signalSemaphoreOffset = semaphores.size();

		for (uint32_t j = 0; j < submitInfo[i].m_signalSemaphoreCount; ++j)
		{
			semaphores.push_back((VkSemaphore)submitInfo[i].m_signalSemaphores[j]->getNativeHandle());
		}

		auto &timelineSubInfo = timelineSemaphoreInfoVk[i];
		timelineSubInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubInfo.waitSemaphoreValueCount = submitInfo[i].m_waitSemaphoreCount;
		timelineSubInfo.pWaitSemaphoreValues = submitInfo[i].m_waitValues;
		timelineSubInfo.signalSemaphoreValueCount = submitInfo[i].m_signalSemaphoreCount;
		timelineSubInfo.pSignalSemaphoreValues = submitInfo[i].m_signalValues;

		auto &subInfo = submitInfoVk[i];
		subInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		subInfo.pNext = &timelineSubInfo;
		subInfo.waitSemaphoreCount = submitInfo[i].m_waitSemaphoreCount;
		subInfo.pWaitSemaphores = semaphores.data() + waitSemaphoreOffset;
		subInfo.pWaitDstStageMask = submitInfo[i].m_waitDstStageMask;
		subInfo.commandBufferCount = submitInfo[i].m_commandListCount;
		subInfo.pCommandBuffers = commandBuffers.data() + commandBuffersOffset;
		subInfo.signalSemaphoreCount = submitInfo[i].m_signalSemaphoreCount;
		subInfo.pSignalSemaphores = semaphores.data() + signalSemaphoreOffset;
	}

	UtilityVk::checkResult(vkQueueSubmit(m_queue, count, submitInfoVk.data(), VK_NULL_HANDLE), "Failed to submit to Queue!");
}
