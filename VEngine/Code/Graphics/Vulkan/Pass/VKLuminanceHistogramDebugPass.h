#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKLuminanceHistogramDebugPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_offsetX;
			float m_offsetY;
			float m_scaleX;
			float m_scaleY;
			FrameGraph::ImageHandle m_colorImageHandle;
			FrameGraph::BufferHandle m_luminanceHistogramBufferHandle;
		};
		
		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}