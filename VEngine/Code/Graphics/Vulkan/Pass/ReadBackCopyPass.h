#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ReadBackCopyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			VkBufferCopy m_bufferCopy;
			BufferViewHandle m_srcBuffer;
			VkBuffer m_dstBuffer;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}