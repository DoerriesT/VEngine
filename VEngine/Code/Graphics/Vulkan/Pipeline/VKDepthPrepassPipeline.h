#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKDepthPrepassPipeline
	{
	public:
		explicit VKDepthPrepassPipeline();
		VKDepthPrepassPipeline(const VKDepthPrepassPipeline &) = delete;
		VKDepthPrepassPipeline(const VKDepthPrepassPipeline &&) = delete;
		VKDepthPrepassPipeline &operator= (const VKDepthPrepassPipeline &) = delete;
		VKDepthPrepassPipeline &operator= (const VKDepthPrepassPipeline &&) = delete;
		~VKDepthPrepassPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const std::vector<DrawItem> &drawItems);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
