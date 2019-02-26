#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKGeometryPipeline
	{
	public:
		explicit VKGeometryPipeline(VkDevice device, VKRenderResources *renderResources, bool alphaMasked);
		VKGeometryPipeline(const VKGeometryPipeline &) = delete;
		VKGeometryPipeline(const VKGeometryPipeline &&) = delete;
		VKGeometryPipeline &operator= (const VKGeometryPipeline &) = delete;
		VKGeometryPipeline &operator= (const VKGeometryPipeline &&) = delete;
		~VKGeometryPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;
		
	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
