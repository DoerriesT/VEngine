#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ExposurePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::BufferViewHandle m_luminanceHistogramBufferHandle;
			rg::BufferViewHandle m_avgLuminanceBufferHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}