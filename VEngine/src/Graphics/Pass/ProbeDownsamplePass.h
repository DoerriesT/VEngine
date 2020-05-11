#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeDownsamplePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_resultImageViewHandles[7]; // 7 mips with 6 layers each (array view)
			
			// we need to use the same mips both as array and as cube, but RenderGraph can't handle multiple
			// virtual views to the same subresources in a single pass, so we use our own cube views
			gal::ImageView *m_cubeImageViews[7];
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}