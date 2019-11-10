#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace UpdateQueueProbabilityPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_prevQueueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}