#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKTriangleFilterPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_clusterOffset;
			uint32_t m_clusterCount;
			uint32_t m_matrixBufferOffset;
			bool m_cullBackface;
			VkDescriptorBufferInfo m_clusterInfoBufferInfo;
			VkDescriptorBufferInfo m_inputIndicesBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_positionsBufferInfo;
			VkDescriptorBufferInfo m_indexOffsetsBufferInfo;
			FrameGraph::BufferHandle m_indexCountsBufferHandle;
			FrameGraph::BufferHandle m_filteredIndicesBufferHandle;
			FrameGraph::BufferHandle m_viewProjectionMatrixBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}