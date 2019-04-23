#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	struct VKRenderResources;

	namespace VKMemoryHeapDebugPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_offsetX;
			float m_offsetY;
			float m_scaleX;
			float m_scaleY;
			FrameGraph::ImageHandle m_colorImageHandle;

		};
		
		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}