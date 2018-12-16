#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;

	class VKForwardPipeline
	{
	public:
		explicit VKForwardPipeline();
		VKForwardPipeline(const VKForwardPipeline &) = delete;
		VKForwardPipeline(const VKForwardPipeline &&) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &&) = delete;
		~VKForwardPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
