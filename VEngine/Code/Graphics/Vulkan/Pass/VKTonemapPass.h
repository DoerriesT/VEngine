#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKTonemapPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_resourceIndex;
			FrameGraph::ImageHandle m_srcImageHandle;
			FrameGraph::ImageHandle m_dstImageHandle;
			FrameGraph::BufferHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}