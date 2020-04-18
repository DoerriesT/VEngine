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
			bool m_ignoreHistory;
			float m_jitterOffsetX;
			float m_jitterOffsetY;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_taaHistoryImageHandle;
			rg::ImageViewHandle m_lightImageHandle;
			rg::ImageViewHandle m_taaResolveImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}