#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BrickDebugPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_brickPtrImageHandle;
			ImageViewHandle m_colorImageHandle;
			BufferViewHandle m_binVisBricksBufferHandle;
			BufferViewHandle m_colorBricksBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}