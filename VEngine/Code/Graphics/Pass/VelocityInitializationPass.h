#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VelocityInitializationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}