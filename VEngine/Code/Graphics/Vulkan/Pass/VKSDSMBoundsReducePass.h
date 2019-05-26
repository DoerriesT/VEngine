#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKSDSMBoundsReducePass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			glm::mat4 m_invProjection;
			FrameGraph::BufferHandle m_partitionBoundsBufferHandle;
			FrameGraph::BufferHandle m_splitsBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}