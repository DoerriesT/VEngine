#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;

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
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
