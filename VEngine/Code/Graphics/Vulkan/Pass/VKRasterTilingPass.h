#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct VKRenderResources;
	struct LightData;

	class VKRasterTilingPass : FrameGraph::Pass
	{
	public:
		explicit VKRasterTilingPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			const LightData &lightData,
			const glm::mat4 &viewProjection);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle &pointLightBitMaskBufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		const LightData &m_lightData;
		glm::mat4 m_viewProjection;
		VKGraphicsPipelineDescription m_pipelineDesc;
	};
}