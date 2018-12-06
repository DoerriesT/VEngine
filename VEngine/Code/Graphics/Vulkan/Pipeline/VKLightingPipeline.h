#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKLightingPipeline
	{
	public:
		explicit VKLightingPipeline();
		VKLightingPipeline(const VKLightingPipeline &) = delete;
		VKLightingPipeline(const VKLightingPipeline &&) = delete;
		VKLightingPipeline &operator= (const VKLightingPipeline &) = delete;
		VKLightingPipeline &operator= (const VKLightingPipeline &&) = delete;
		~VKLightingPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
