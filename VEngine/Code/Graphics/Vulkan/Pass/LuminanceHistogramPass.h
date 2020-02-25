#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LuminanceHistogramPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_lightImageHandle;
			BufferViewHandle m_luminanceHistogramBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}