#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_bias;
			uint32_t m_noiseTextureHandle;
			ImageViewHandle m_hiZPyramidImageHandle;
			ImageViewHandle m_normalImageHandle;
			ImageViewHandle m_rayHitPDFImageHandle;
			ImageViewHandle m_maskImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}
