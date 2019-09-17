#pragma once
#include "volk.h"
#include <vector>

namespace VEngine
{
	class DeferredObjectDeleter
	{
	public:
		DeferredObjectDeleter() = default;
		DeferredObjectDeleter(DeferredObjectDeleter &) = delete;
		DeferredObjectDeleter(DeferredObjectDeleter &&) = delete;
		DeferredObjectDeleter &operator= (const DeferredObjectDeleter &) = delete;
		DeferredObjectDeleter &operator= (const DeferredObjectDeleter &&) = delete;
		~DeferredObjectDeleter();
		void update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease);
		void add(VkFramebuffer framebuffer);

	private:
		uint64_t m_frameIndex;
		std::vector<std::pair<VkFramebuffer, uint64_t>> m_framebuffers;
	};
}