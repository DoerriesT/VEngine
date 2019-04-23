#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	namespace VKBlitPass
	{
		struct Data
		{
			uint32_t m_regionCount;
			const VkImageBlit *m_regions;
			VkFilter m_filter;
			FrameGraph::ImageHandle m_srcImage;
			FrameGraph::ImageHandle m_dstImage;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data, const char *name);
	}
}