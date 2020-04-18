#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GTAOTemporalFilterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ignoreHistory;
			rg::ImageViewHandle m_inputImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_previousImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}