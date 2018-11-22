#pragma once
#include "VKContext.h"
#include <memory>

namespace VEngine
{
	struct VKRenderResources;
	class VKSwapChain;
	class VKForwardPipeline;

	class VKRenderer
	{
	public:
		explicit VKRenderer();
		~VKRenderer();
		void init(unsigned int width, unsigned int height);
		void update();
		void render();

	private:
		unsigned int m_width;
		unsigned int m_height;
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<VKSwapChain> m_swapChain;

		std::unique_ptr<VKForwardPipeline> m_forwardPipeline;
	};
}