#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKShadowPipeline
	{
	public:
		explicit VKShadowPipeline(VkDevice device, VKRenderResources *renderResources);
		VKShadowPipeline(const VKShadowPipeline &) = delete;
		VKShadowPipeline(const VKShadowPipeline &&) = delete;
		VKShadowPipeline &operator= (const VKShadowPipeline &) = delete;
		VKShadowPipeline &operator= (const VKShadowPipeline &&) = delete;
		~VKShadowPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;

	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
