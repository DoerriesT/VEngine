#pragma once
#include <cstdint>
#include "gal/FwdDecl.h"

namespace VEngine
{
	class GALTestRenderer
	{
	public:
		explicit GALTestRenderer(uint32_t width, uint32_t height, void *windowHandle);
		~GALTestRenderer();
		void render();
	private:
		uint32_t m_width;
		uint32_t m_height;
		uint64_t m_frameIndex;
		uint64_t m_semaphoreValue;
		uint64_t m_frameSemaphoreValues[2];
		gal::GraphicsDevice *m_graphicsDevice;
		gal::Queue *m_queue;
		gal::SwapChain *m_swapChain;
		gal::Semaphore *m_semaphore;
		gal::GraphicsPipeline *m_pipeline;
		gal::Buffer *m_vertexBuffer;
		gal::Buffer *m_indexBuffer;
		gal::CommandListPool *m_cmdListPools[2];
		gal::CommandList *m_cmdLists[2];
		gal::ImageView *m_imageViews[2];
	};
}