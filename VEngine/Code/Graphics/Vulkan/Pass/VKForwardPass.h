#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKForwardPass : FrameGraph::Pass
	{
	public:
		explicit VKForwardPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			size_t drawItemCount,
			const DrawItem *drawItems,
			uint32_t drawItemBufferOffset);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle perDrawDataBufferHandle,
			FrameGraph::BufferHandle pointLightZBinsBufferHandle,
			FrameGraph::BufferHandle pointLightBitMaskBufferHandle,
			FrameGraph::BufferHandle directionalLightDataBufferHandle,
			FrameGraph::BufferHandle pointLightDataBufferHandle,
			FrameGraph::ImageHandle shadowTextureHandle,
			FrameGraph::ImageHandle &depthTextureHandle,
			FrameGraph::ImageHandle &lightTextureHandle,
			FrameGraph::ImageHandle &velocityTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		size_t m_drawItemCount;
		const DrawItem *m_drawItems;
		uint32_t m_drawItemBufferOffset;
	};
}