#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRTemporalFilterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_resultImageViewHandle;
			ImageViewHandle m_historyImageViewHandle;
			ImageViewHandle m_colorRayDepthImageViewHandle;
			ImageViewHandle m_maskImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}