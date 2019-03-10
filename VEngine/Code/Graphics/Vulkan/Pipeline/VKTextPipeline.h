#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKTextPipeline
	{
	public:
		explicit VKTextPipeline(VkDevice device, VKRenderResources *renderResources, VkFormat attachmentFormat);
		VKTextPipeline(const VKTextPipeline &) = delete;
		VKTextPipeline(const VKTextPipeline &&) = delete;
		VKTextPipeline &operator= (const VKTextPipeline &) = delete;
		VKTextPipeline &operator= (const VKTextPipeline &&) = delete;
		~VKTextPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;

	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
