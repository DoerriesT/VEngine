#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;
	struct SubMeshData;
	struct SubMeshInstanceData;
	struct ShadowJob;

	namespace VKShadowPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			size_t m_subMeshInstanceCount;
			const SubMeshInstanceData *m_subMeshInstances;
			const SubMeshData *m_subMeshData;
			uint32_t m_shadowJobCount;
			const ShadowJob *m_shadowJobs;
			bool m_alphaMasked;
			bool m_clear;
			FrameGraph::BufferHandle m_transformDataBufferHandle;
			FrameGraph::BufferHandle m_materialDataBufferHandle;
			FrameGraph::ImageHandle m_shadowAtlasImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}