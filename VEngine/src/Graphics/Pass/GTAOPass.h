#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GTAOPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_tangentSpaceImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}