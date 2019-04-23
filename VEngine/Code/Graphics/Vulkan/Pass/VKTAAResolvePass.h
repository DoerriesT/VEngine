#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKTAAResolvePass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_jitterOffsetX;
			float m_jitterOffsetY;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_velocityImageHandle;
			FrameGraph::ImageHandle m_taaHistoryImageHandle;
			FrameGraph::ImageHandle m_lightImageHandle;
			FrameGraph::ImageHandle m_taaResolveImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}