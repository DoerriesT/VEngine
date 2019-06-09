#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKDrawCallCompactionPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_drawCallOffset;
			uint32_t m_drawCallCount;
			uint32_t m_instanceOffset;
			uint32_t m_drawCountsOffset;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshDataBufferInfo;
			VkDescriptorBufferInfo m_indexOffsetsBufferInfo;
			FrameGraph::BufferHandle m_indexCountsBufferHandle;
			FrameGraph::BufferHandle m_indirectBufferHandle;
			FrameGraph::BufferHandle m_drawCountsBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}