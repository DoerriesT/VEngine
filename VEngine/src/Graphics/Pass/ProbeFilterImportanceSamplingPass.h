#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeFilterImportanceSamplingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle; // cube with 7 mips
			gal::ImageView *m_resultImageViews[7]; // 7 mips with 6 layers each
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}