#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DebugDrawData;

	namespace DebugDrawPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const DebugDrawData *m_debugDrawData;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}