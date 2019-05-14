#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include "Graphics/RenderData.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;

	namespace VKPrepareIndirectBuffersPass
	{
		struct Data
		{
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_opaqueCount;
			uint32_t m_maskedCount;
			uint32_t m_transparentCount;
			uint32_t m_opaqueShadowCount;
			uint32_t m_maskedShadowCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshDataBufferInfo;
			FrameGraph::BufferHandle m_opaqueIndirectBufferHandle;
			FrameGraph::BufferHandle m_maskedIndirectBufferHandle;
			FrameGraph::BufferHandle m_transparentIndirectBufferHandle;
			FrameGraph::BufferHandle m_opaqueShadowIndirectBufferHandle;
			FrameGraph::BufferHandle m_maskedShadowIndirectBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}