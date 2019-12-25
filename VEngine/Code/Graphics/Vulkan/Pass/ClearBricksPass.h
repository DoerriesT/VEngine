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
			BufferViewHandle m_binVisBricksBufferHandle;
			BufferViewHandle m_colorBricksBufferHandle;
			BufferViewHandle m_freeBricksBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}