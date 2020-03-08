#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace SSRTemporalFilterPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
			rg::ImageViewHandle m_colorRayDepthImageViewHandle;
			rg::ImageViewHandle m_maskImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}