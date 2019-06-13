#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;
	struct CommonRenderData;

	namespace VKDirectLightingPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			const CommonRenderData *m_commonRenderData;
			uint32_t m_width;
			uint32_t m_height;
			bool m_ssao;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			FrameGraph::BufferHandle m_shadowDataBufferHandle;
			FrameGraph::BufferHandle m_pointLightBitMaskBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_uvImageHandle;
			FrameGraph::ImageHandle m_ddxyLengthImageHandle;
			FrameGraph::ImageHandle m_ddxyRotMaterialIdImageHandle;
			FrameGraph::ImageHandle m_tangentSpaceImageHandle;
			FrameGraph::ImageHandle m_shadowAtlasImageHandle;
			FrameGraph::ImageHandle m_occlusionImageHandle;
			FrameGraph::ImageHandle m_resultImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}