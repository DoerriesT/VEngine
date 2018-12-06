#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;

	class VKGeometryAlphaMaskPipeline
	{
	public:
		explicit VKGeometryAlphaMaskPipeline();
		VKGeometryAlphaMaskPipeline(const VKGeometryAlphaMaskPipeline &) = delete;
		VKGeometryAlphaMaskPipeline(const VKGeometryAlphaMaskPipeline &&) = delete;
		VKGeometryAlphaMaskPipeline &operator= (const VKGeometryAlphaMaskPipeline &) = delete;
		VKGeometryAlphaMaskPipeline &operator= (const VKGeometryAlphaMaskPipeline &&) = delete;
		~VKGeometryAlphaMaskPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResources);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
