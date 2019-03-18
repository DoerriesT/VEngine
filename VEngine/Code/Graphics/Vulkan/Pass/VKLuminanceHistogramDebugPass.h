#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKLuminanceHistogramDebugPass : FrameGraph::Pass
	{
	public:
		explicit VKLuminanceHistogramDebugPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			float offsetX,
			float offsetY,
			float scaleX,
			float scaleY);

		void addToGraph(FrameGraph::Graph &graph, 
			FrameGraph::BufferHandle perFrameDataBufferHandle, 
			FrameGraph::ImageHandle colorTextureHandle, 
			FrameGraph::BufferHandle luminanceHistogramBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		float m_offsetX;
		float m_offsetY;
		float m_scaleX;
		float m_scaleY;
		VKGraphicsPipelineDescription m_pipelineDesc;
	};
}