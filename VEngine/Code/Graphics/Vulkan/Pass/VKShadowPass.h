#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;
	struct ShadowJob;

	class VKShadowPass : FrameGraph::Pass
	{
	public:
		explicit VKShadowPass(
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			size_t drawItemCount,
			const DrawItem *drawItems,
			uint32_t drawItemOffset,
			size_t shadowJobCount,
			const ShadowJob *shadowJobs);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle perDrawDataBufferHandle,
			FrameGraph::ImageHandle shadowTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		size_t m_drawItemCount;
		const DrawItem *m_drawItems;
		uint32_t m_drawItemOffset;
		size_t m_shadowJobCount;
		const ShadowJob *m_shadowJobs;
		VKGraphicsPipelineDescription m_pipelineDesc;
	};
}