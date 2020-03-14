#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LuminanceHistogramAveragePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::BufferViewHandle m_luminanceHistogramBufferHandle;
			rg::BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}