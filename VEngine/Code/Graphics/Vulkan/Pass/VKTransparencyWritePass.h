#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include "Graphics/Vulkan/VKPipeline.h"
#include "Graphics/RenderData.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKTransparencyWritePass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			const CommonRenderData *m_commonRenderData;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_drawCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_shadowDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			FrameGraph::BufferHandle m_pointLightBitMaskBufferHandle;
			FrameGraph::BufferHandle m_indirectBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_shadowAtlasImageHandle;
			FrameGraph::ImageHandle m_lightImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}