#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKVelocityInitializationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_velocityImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}