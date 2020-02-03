#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRResolvePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_rayHitPDFImageHandle;
			ImageViewHandle m_maskImageHandle;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_normalImageHandle;
			ImageViewHandle m_prevColorImageHandle;
			ImageViewHandle m_velocityImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}
