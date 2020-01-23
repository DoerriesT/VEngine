#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace InitBrickPoolPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			BufferViewHandle m_freeBricksBufferHandle;
			ImageViewHandle m_binVisBricksImageHandle;
			ImageViewHandle m_colorBricksImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}