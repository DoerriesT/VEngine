#pragma once
#include "volk.h"
#include <unordered_map>
#include "VKPipeline.h"

namespace VEngine
{
	class RenderPassCache
	{
	public:
		void getRenderPass(const RenderPassDesc &renderPassDesc, RenderPassCompatDesc &compatDesc, VkRenderPass &renderPass);

	private:
		std::unordered_map<RenderPassDesc, VkRenderPass, RenderPassDescHash> m_renderPasses;
	};
}