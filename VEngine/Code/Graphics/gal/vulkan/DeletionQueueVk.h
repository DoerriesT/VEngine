#pragma once
#include "Graphics/Vulkan/volk.h"
#include <vector>

namespace VEngine
{
	namespace gal
	{
		class DeletionQueueVk
		{
		public:
			explicit DeletionQueueVk(VkDevice device, uint32_t frameDelay);
			DeletionQueueVk(DeletionQueueVk &) = delete;
			DeletionQueueVk(DeletionQueueVk &&) = delete;
			DeletionQueueVk &operator= (const DeletionQueueVk &) = delete;
			DeletionQueueVk &operator= (const DeletionQueueVk &&) = delete;
			~DeletionQueueVk();
			void update(uint64_t frameIndex);
			void add(VkFramebuffer framebuffer);
		private:
			VkDevice m_device;
			uint64_t m_frameIndex;
			uint32_t m_frameDelay;
			std::vector<std::vector<VkFramebuffer>> m_framebuffers;
		};
	}
}