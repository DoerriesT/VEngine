#pragma once
#include "Graphics/RenderGraph.h"

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
			rg::ImageViewHandle m_hiZPyramidImageHandle;
			rg::ImageViewHandle m_normalImageHandle;
			rg::ImageViewHandle m_specularRoughnessImageHandle;
			rg::ImageViewHandle m_rayHitPDFImageHandle;
			rg::ImageViewHandle m_maskImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}
