#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;
	struct SubMeshData;
	struct SubMeshInstanceData;
	struct ShadowJob;

	class VKShadowPass : FrameGraph::Pass
	{
	public:
		explicit VKShadowPass(
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			size_t resourceIndex,
			size_t subMeshInstanceCount,
			const SubMeshInstanceData *subMeshInstances,
			const SubMeshData *subMeshData,
			size_t shadowJobCount,
			const ShadowJob *shadowJobs);

		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::BufferHandle perFrameDataBufferHandle,
			FrameGraph::BufferHandle transformDataBufferHandle,
			FrameGraph::ImageHandle shadowTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		size_t m_resourceIndex;
		size_t m_subMeshInstanceCount;
		const SubMeshInstanceData *m_subMeshInstances;
		const SubMeshData *m_subMeshData;
		size_t m_shadowJobCount;
		const ShadowJob *m_shadowJobs;
		VKGraphicsPipelineDescription m_pipelineDesc;
	};
}