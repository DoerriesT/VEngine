#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LuminanceHistogramPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_lightImageHandle;
			rg::BufferViewHandle m_luminanceHistogramBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}