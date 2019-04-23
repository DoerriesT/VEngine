#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include "Graphics/Vulkan/VKPipeline.h"
#include "Graphics/RenderData.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;
	struct SubMeshData;
	struct SubMeshInstanceData;

	namespace VKGeometryPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			const CommonRenderData *m_commonRenderData;
			uint32_t m_width;
			uint32_t m_height;
			size_t m_subMeshInstanceCount;
			const SubMeshInstanceData *m_subMeshInstances;
			const SubMeshData *m_subMeshData;
			bool m_alphaMasked;
			FrameGraph::BufferHandle m_constantDataBufferHandle;
			FrameGraph::BufferHandle m_materialDataBufferHandle;
			FrameGraph::BufferHandle m_transformDataBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_albedoImageHandle;
			FrameGraph::ImageHandle m_normalImageHandle;
			FrameGraph::ImageHandle m_metalnessRougnessOcclusionImageHandle;
			FrameGraph::ImageHandle m_velocityImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, Data &data);
	}
}