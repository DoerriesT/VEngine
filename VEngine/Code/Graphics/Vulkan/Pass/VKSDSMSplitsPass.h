#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMSplitsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_depthBoundsBufferHandle;
			BufferViewHandle m_splitsBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}