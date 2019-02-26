#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKLightingPass : FrameGraph::Pass
	{
	public:
		explicit VKLightingPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle directionalLightDataBufferHandle,
			FrameGraph::BufferHandle pointLightDataBufferHandle,
			FrameGraph::BufferHandle spotLightDataBufferHandle,
			FrameGraph::BufferHandle shadowDataBufferHandle,
			FrameGraph::BufferHandle pointLightZBinsBufferHandle,
			FrameGraph::BufferHandle pointLightBitMaskBufferHandle,
			FrameGraph::ImageHandle depthTextureHandle,
			FrameGraph::ImageHandle albedoTextureHandle,
			FrameGraph::ImageHandle normalTextureHandle,
			FrameGraph::ImageHandle materialTextureHandle,
			FrameGraph::ImageHandle shadowTextureHandle,
			FrameGraph::ImageHandle &lightTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
	};
}