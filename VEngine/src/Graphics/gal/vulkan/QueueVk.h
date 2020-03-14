#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace VEngine
{
	namespace gal
	{
		class QueueVk : public Queue
		{
		public:
			void *getNativeHandle() const override;
			uint32_t getFamilyIndex() const override;
			uint32_t getTimestampValidBits() const override;
			void submit(uint32_t count, const SubmitInfo *submitInfo) override;
			void waitIdle() const override;

			uint32_t m_queueFamily = -1;
			VkQueue m_queue = VK_NULL_HANDLE;
			uint32_t m_timestampValidBits = 0;
			bool m_presentable = false;
		};
	}
}