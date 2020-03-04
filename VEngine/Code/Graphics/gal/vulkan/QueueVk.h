#pragma once
#include "Graphics/gal/Queue.h"
#include <cstdint>
#include "Graphics/Vulkan/volk.h"

namespace VEngine
{
	namespace gal
	{
		class QueueVk : public Queue
		{
		public:
			void *getNativeHandle() const override;
			uint32_t getFamilyIndex() const override;
			void submit(uint32_t count, const SubmitInfo *submitInfo) override;

			uint32_t m_queueFamily = -1;
			VkQueue m_queue = VK_NULL_HANDLE;
			bool m_presentable = false;
		};
	}
}