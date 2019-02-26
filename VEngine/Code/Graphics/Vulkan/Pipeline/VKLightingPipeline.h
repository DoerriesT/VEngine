#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKLightingPipeline
	{
	public:
		explicit VKLightingPipeline(VkDevice device, VKRenderResources *renderResources);
		VKLightingPipeline(const VKLightingPipeline &) = delete;
		VKLightingPipeline(const VKLightingPipeline &&) = delete;
		VKLightingPipeline &operator= (const VKLightingPipeline &) = delete;
		VKLightingPipeline &operator= (const VKLightingPipeline &&) = delete;
		~VKLightingPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;

	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
