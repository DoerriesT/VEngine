#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LuminanceHistogramAveragePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_luminanceHistogramBufferHandle;
			BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}