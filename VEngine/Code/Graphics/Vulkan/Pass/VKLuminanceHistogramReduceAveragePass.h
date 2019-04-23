#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKLuminanceHistogramAveragePass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_timeDelta;
			float m_logMin;
			float m_logMax;
			uint32_t m_currentResourceIndex;
			uint32_t m_previousResourceIndex;
			FrameGraph::BufferHandle m_luminanceHistogramBufferHandle;
			FrameGraph::BufferHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}