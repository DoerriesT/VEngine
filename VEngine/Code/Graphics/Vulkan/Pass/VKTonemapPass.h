#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKTonemapPass : FrameGraph::Pass
	{
	public:
		explicit VKTonemapPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle srcImageHandle,
			FrameGraph::ImageHandle dstImageHandle,
			FrameGraph::BufferHandle avgLuminanceBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		VKComputePipelineDescription m_pipelineDesc;
	};
}