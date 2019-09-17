#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMClearPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_depthBoundsBufferHandle;
			BufferViewHandle m_partitionBoundsBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}