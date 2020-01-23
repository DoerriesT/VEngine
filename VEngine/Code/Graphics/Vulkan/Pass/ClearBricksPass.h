#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ClearBricksPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_brickPointerImageHandle;
			ImageViewHandle m_binVisBricksImageHandle;
			ImageViewHandle m_colorBricksImageHandle;
			BufferViewHandle m_freeBricksBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}