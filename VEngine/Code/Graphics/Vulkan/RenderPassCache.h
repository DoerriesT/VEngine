#pragma once
#include "volk.h"
#include <unordered_map>
#include "VKPipeline.h"

namespace VEngine
{
	class RenderPassCache
	{
	public:
		void getRenderPass(const RenderPassDescription &renderPassDesc, RenderPassCompatibilityDescription &compatDesc, VkRenderPass &renderPass);

	private:
		std::unordered_map<RenderPassDescription, VkRenderPass, RenderPassDescriptionHash> m_graphicsPipelines;
	};
}