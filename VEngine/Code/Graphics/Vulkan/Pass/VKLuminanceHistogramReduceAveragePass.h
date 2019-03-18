#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKLuminanceHistogramReduceAveragePass : FrameGraph::Pass
	{
	public:
		explicit VKLuminanceHistogramReduceAveragePass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			float timeDelta);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle luminanceHistogramBufferHandle,
			FrameGraph::BufferHandle avgLuminanceBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		float m_timeDelta;
		VKComputePipelineDescription m_pipelineDesc;
	};
}