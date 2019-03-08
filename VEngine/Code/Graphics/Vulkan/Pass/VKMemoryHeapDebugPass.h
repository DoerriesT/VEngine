#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKMemoryHeapDebugPass : FrameGraph::Pass
	{
	public:
		explicit VKMemoryHeapDebugPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			uint32_t width,
			uint32_t height,
			float offsetX,
			float offsetY,
			float scaleX,
			float scaleY);

		void addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle colorTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		uint32_t m_width;
		uint32_t m_height;
		float m_offsetX;
		float m_offsetY;
		float m_scaleX;
		float m_scaleY;;
	};
}