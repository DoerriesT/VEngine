#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_hiZPyramidImageHandle;
			ImageViewHandle m_normalImageHandle;
			ImageViewHandle m_prevColorImageHandle;
			ImageViewHandle m_velocityImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}
