#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace VEngine
{
	namespace gal
	{
		class QueueDx12 : public Queue
		{
		public:
			void *getNativeHandle() const override;
			uint32_t getFamilyIndex() const override;
			uint32_t getTimestampValidBits() const override;
			void submit(uint32_t count, const SubmitInfo *submitInfo) override;
			void waitIdle() const override;

			uint32_t m_queueFamily = -1;
			ID3D12CommandQueue *m_queue;
			uint32_t m_timestampValidBits = 0;
			bool m_presentable = false;

		private:
			Semaphore *m_waitIdleSemaphore;
			mutable uint64_t m_semaphoreValue;
		};
	}
}