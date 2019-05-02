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
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshDataBufferInfo;
			FrameGraph::BufferHandle m_opaqueIndirectBufferHandle;
			FrameGraph::BufferHandle m_maskedIndirectBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}