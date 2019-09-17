#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMDepthReducePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_depthBoundsBufferHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}