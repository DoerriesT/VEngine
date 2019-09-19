#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LuminanceHistogramReadBackCopyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_srcBuffer;
			VkBuffer m_dstBuffer;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}