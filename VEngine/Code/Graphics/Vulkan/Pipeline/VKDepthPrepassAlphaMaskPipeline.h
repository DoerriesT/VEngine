#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKDepthPrepassAlphaMaskPipeline
	{
	public:
		explicit VKDepthPrepassAlphaMaskPipeline();
		VKDepthPrepassAlphaMaskPipeline(const VKDepthPrepassAlphaMaskPipeline &) = delete;
		VKDepthPrepassAlphaMaskPipeline(const VKDepthPrepassAlphaMaskPipeline &&) = delete;
		VKDepthPrepassAlphaMaskPipeline &operator= (const VKDepthPrepassAlphaMaskPipeline &) = delete;
		VKDepthPrepassAlphaMaskPipeline &operator= (const VKDepthPrepassAlphaMaskPipeline &&) = delete;
		~VKDepthPrepassAlphaMaskPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, uint32_t previousOffset, const std::vector<DrawItem> &drawItems);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
