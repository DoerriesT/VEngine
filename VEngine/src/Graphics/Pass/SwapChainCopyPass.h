#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SwapChainCopyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle;
			gal::ImageView *m_resultImageView;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}