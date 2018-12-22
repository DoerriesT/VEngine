#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKTilingPipeline
	{
	public:
		explicit VKTilingPipeline();
		VKTilingPipeline(const VKTilingPipeline &) = delete;
		VKTilingPipeline(const VKTilingPipeline &&) = delete;
		VKTilingPipeline &operator= (const VKTilingPipeline &) = delete;
		VKTilingPipeline &operator= (const VKTilingPipeline &&) = delete;
		~VKTilingPipeline();
		void init(VKRenderResources *renderResources);
		void recordCommandBuffer(VKRenderResources *renderResources, uint32_t width, uint32_t height);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
