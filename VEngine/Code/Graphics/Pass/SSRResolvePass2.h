#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace SSRResolvePass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			float m_bias;
			uint32_t m_noiseTextureHandle;
			rg::ImageViewHandle m_rayHitPDFImageHandle;
			rg::ImageViewHandle m_maskImageHandle;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_normalImageHandle;
			rg::ImageViewHandle m_albedoImageHandle;
			rg::ImageViewHandle m_prevColorImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
			rg::ImageViewHandle m_resultMaskImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}
