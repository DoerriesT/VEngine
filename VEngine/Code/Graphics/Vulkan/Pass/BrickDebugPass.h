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
			ImageViewHandle m_binVisBricksImageHandle;
			ImageViewHandle m_colorBricksImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}