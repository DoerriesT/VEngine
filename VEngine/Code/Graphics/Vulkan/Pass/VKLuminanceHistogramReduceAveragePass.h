#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKLuminanceHistogramAveragePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_logMin;
			float m_logMax;
			BufferViewHandle m_luminanceHistogramBufferHandle;
			BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}