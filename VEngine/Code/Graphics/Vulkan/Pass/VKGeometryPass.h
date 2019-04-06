#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;
	struct SubMeshData;
	struct SubMeshInstanceData;

	class VKGeometryPass : FrameGraph::Pass
	{
	public:
		explicit VKGeometryPass(VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			size_t subMeshInstanceCount,
			const SubMeshInstanceData *subMeshInstances,
			const SubMeshData *subMeshData,
			bool alphaMasked);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle materialDataBufferHandle,
			FrameGraph::BufferHandle transformDataBufferHandle,
			FrameGraph::ImageHandle depthTextureHandle,
			FrameGraph::ImageHandle albedoTextureHandle,
			FrameGraph::ImageHandle normalTextureHandle,
			FrameGraph::ImageHandle materialTextureHandle,
			FrameGraph::ImageHandle velocityTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		size_t m_subMeshInstanceCount;
		const SubMeshInstanceData *m_subMeshInstances;
		const SubMeshData *m_subMeshData;
		bool m_alphaMasked;
		VKGraphicsPipelineDescription m_pipelineDesc;
	};
}