#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKLuminanceHistogramPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_logMin;
			float m_logMax;
			ImageViewHandle m_lightImageHandle;
			BufferViewHandle m_luminanceHistogramBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}