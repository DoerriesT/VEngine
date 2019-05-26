#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKSDSMDepthReducePass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			FrameGraph::BufferHandle m_depthBoundsBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}