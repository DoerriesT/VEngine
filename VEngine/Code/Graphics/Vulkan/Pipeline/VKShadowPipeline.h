#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;
	struct LightData;

	class VKShadowPipeline
	{
	public:
		explicit VKShadowPipeline();
		VKShadowPipeline(const VKShadowPipeline &) = delete;
		VKShadowPipeline(const VKShadowPipeline &&) = delete;
		VKShadowPipeline &operator= (const VKShadowPipeline &) = delete;
		VKShadowPipeline &operator= (const VKShadowPipeline &&) = delete;
		~VKShadowPipeline();
		void init(VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists, const LightData &lightData);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
