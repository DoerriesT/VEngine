#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace SSRPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			float m_bias;
			uint32_t m_noiseTextureHandle;
			rg::ImageViewHandle m_hiZPyramidImageHandle;
			rg::ImageViewHandle m_normalImageHandle;
			rg::ImageViewHandle m_rayHitPDFImageHandle;
			rg::ImageViewHandle m_maskImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}
