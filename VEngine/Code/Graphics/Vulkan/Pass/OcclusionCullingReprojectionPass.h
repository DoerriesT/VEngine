#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace OcclusionCullingReprojectionPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_prevDepthImageViewHandle;
			ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}