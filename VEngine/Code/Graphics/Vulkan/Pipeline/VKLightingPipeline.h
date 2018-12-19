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
		void init(VKRenderResources *renderResources);
		void recordCommandBuffer(VKRenderResources *renderResources, uint32_t width, uint32_t height);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
