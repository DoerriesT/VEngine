#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKTilingPass : FrameGraph::Pass
	{
	public:
		explicit VKTilingPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			uint32_t pointLightCount);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle pointLightCullDataBufferHandle,
			FrameGraph::BufferHandle &pointLightBitMaskBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		uint32_t m_pointLightCount;
	};
}