#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKGeometryPass : FrameGraph::Pass
	{
	public:
		explicit VKGeometryPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			size_t drawItemCount,
			const DrawItem *drawItems,
			uint32_t drawItemBufferOffset,
			bool alphaMasked);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle perDrawDataBufferHandle,
			FrameGraph::ImageHandle &depthTextureHandle,
			FrameGraph::ImageHandle &albedoTextureHandle,
			FrameGraph::ImageHandle &normalTextureHandle,
			FrameGraph::ImageHandle &materialTextureHandle,
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
		bool m_alphaMasked;
	};
}