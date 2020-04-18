#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRTemporalFilterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ignoreHistory;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
			rg::ImageViewHandle m_colorRayDepthImageViewHandle;
			rg::ImageViewHandle m_maskImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}