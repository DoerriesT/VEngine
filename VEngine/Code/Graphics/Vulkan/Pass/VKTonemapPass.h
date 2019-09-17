#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKTonemapPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_srcImageHandle;
			ImageViewHandle m_dstImageHandle;
			BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}