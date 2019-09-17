#include "DeferredObjectDeleter.h"
#include "VKContext.h"

VEngine::DeferredObjectDeleter::~DeferredObjectDeleter()
{
	vkDeviceWaitIdle(g_context.m_device);
	for (auto fb : m_framebuffers)
	{
		vkDestroyFramebuffer(g_context.m_device, fb.first, nullptr);
	}
}

void VEngine::DeferredObjectDeleter::update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease)
{
	m_frameIndex = currentFrameIndex;
	for (auto fb : m_framebuffers)
	{
		if (fb.second == frameIndexToRelease)
		{
			vkDestroyFramebuffer(g_context.m_device, fb.first, nullptr);
		}
	}
}

void VEngine::DeferredObjectDeleter::add(VkFramebuffer framebuffer)
{
	m_framebuffers.push_back({ framebuffer, m_frameIndex });
}
