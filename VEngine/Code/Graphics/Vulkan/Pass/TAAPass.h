#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace TAAPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_jitterOffsetX;
			float m_jitterOffsetY;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_velocityImageHandle;
			ImageViewHandle m_taaHistoryImageHandle;
			ImageViewHandle m_lightImageHandle;
			ImageViewHandle m_taaResolveImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}