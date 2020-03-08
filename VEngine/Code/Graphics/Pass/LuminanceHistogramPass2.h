#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace LuminanceHistogramPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_lightImageHandle;
			rg::BufferViewHandle m_luminanceHistogramBufferHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}