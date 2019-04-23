#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include "Graphics/RenderData.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKLightingPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			const CommonRenderData *m_commonRenderData;
			uint32_t m_width;
			uint32_t m_height;
			FrameGraph::BufferHandle m_constantDataBufferHandle;
			FrameGraph::BufferHandle m_directionalLightDataBufferHandle;
			FrameGraph::BufferHandle m_pointLightDataBufferHandle;
			FrameGraph::BufferHandle m_shadowDataBufferHandle;
			FrameGraph::BufferHandle m_pointLightZBinsBufferHandle;
			FrameGraph::BufferHandle m_pointLightBitMaskBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_albedoImageHandle;
			FrameGraph::ImageHandle m_normalImageHandle;
			FrameGraph::ImageHandle m_metalnessRougnessOcclusionImageHandle;
			FrameGraph::ImageHandle m_shadowAtlasImageHandle;
			FrameGraph::ImageHandle m_resultImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, Data &data);
	}
}