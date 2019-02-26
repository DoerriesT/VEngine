#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKTilingPipeline
	{
	public:
		explicit VKTilingPipeline(VkDevice device, VKRenderResources *renderResources);
		VKTilingPipeline(const VKTilingPipeline &) = delete;
		VKTilingPipeline(const VKTilingPipeline &&) = delete;
		VKTilingPipeline &operator= (const VKTilingPipeline &) = delete;
		VKTilingPipeline &operator= (const VKTilingPipeline &&) = delete;
		~VKTilingPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;

	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
