#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace TemporalAAPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_jitterOffsetX;
			float m_jitterOffsetY;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_taaHistoryImageHandle;
			rg::ImageViewHandle m_lightImageHandle;
			rg::ImageViewHandle m_taaResolveImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}