#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct VKRenderResources;

	class VKVelocityCompositionPass : FrameGraph::Pass
	{
	public:
		explicit VKVelocityCompositionPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			const glm::mat4 &reprojectionMatrix);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle depthImageHandle,
			FrameGraph::ImageHandle velocityImageHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		glm::mat4 m_reprojectionMatrix;
		VKComputePipelineDescription m_pipelineDesc;
	};
}