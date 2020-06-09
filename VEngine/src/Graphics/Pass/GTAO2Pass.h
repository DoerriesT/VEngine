#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GTAO2Pass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
			rg::ImageViewHandle m_normalImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}