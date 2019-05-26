#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKSDSMSplitsPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			float m_nearPlane;
			float m_farPlane;
			FrameGraph::BufferHandle m_depthBoundsBufferHandle;
			FrameGraph::BufferHandle m_splitsBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}