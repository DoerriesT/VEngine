#include "DeletionQueueVk.h"

VEngine::gal::DeletionQueueVk::DeletionQueueVk(VkDevice device, uint32_t frameDelay)
	:m_device(device),
	m_frameIndex(),
	m_frameDelay(frameDelay),
	m_framebuffers(frameDelay)
{
}

VEngine::gal::DeletionQueueVk::~DeletionQueueVk()
{
	vkDeviceWaitIdle(m_device);
	for (auto &frame : m_framebuffers)
	{
		for (auto &fb : frame)
		{
			vkDestroyFramebuffer(m_device, fb, nullptr);
		}
	}
}

void VEngine::gal::DeletionQueueVk::update(uint64_t frameIndex)
{
	m_frameIndex = frameIndex;
	const size_t deletionIndex = m_frameIndex % m_frameDelay;

	for (auto fb : m_framebuffers[deletionIndex])
	{
		vkDestroyFramebuffer(m_device, fb, nullptr);
	}
	m_framebuffers[deletionIndex].clear();
}

void VEngine::gal::DeletionQueueVk::add(VkFramebuffer framebuffer)
{
	m_framebuffers[m_frameIndex % m_frameDelay].push_back(framebuffer);
}
