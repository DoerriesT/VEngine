#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMBoundsReducePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_partitionBoundsBufferHandle;
			BufferViewHandle m_splitsBufferHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}