#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKLuminanceHistogramPass : FrameGraph::Pass
	{
	public:
		explicit VKLuminanceHistogramPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			float logMin,
			float logMax);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle lightTextureHandle,
			FrameGraph::BufferHandle luminanceHistogramBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		float m_logMin;
		float m_logMax;
		VKComputePipelineDescription m_pipelineDesc;
	};
}