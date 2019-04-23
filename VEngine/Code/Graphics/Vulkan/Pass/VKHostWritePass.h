#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	namespace VKHostWritePass
	{
		struct Data
		{
			const unsigned char *m_data;
			size_t m_srcOffset;
			size_t m_dstOffset;
			size_t m_srcItemSize;
			size_t m_dstItemSize;
			size_t m_srcItemStride;
			size_t m_count;
			FrameGraph::BufferHandle m_bufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data, const char *name);
	}
}