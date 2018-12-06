#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;

	class VKGeometryPipeline
	{
	public:
		explicit VKGeometryPipeline();
		VKGeometryPipeline(const VKGeometryPipeline &) = delete;
		VKGeometryPipeline(const VKGeometryPipeline &&) = delete;
		VKGeometryPipeline &operator= (const VKGeometryPipeline &) = delete;
		VKGeometryPipeline &operator= (const VKGeometryPipeline &&) = delete;
		~VKGeometryPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
