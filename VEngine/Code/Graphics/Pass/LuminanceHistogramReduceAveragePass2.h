#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace LuminanceHistogramAveragePass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::BufferViewHandle m_luminanceHistogramBufferHandle;
			rg::BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}