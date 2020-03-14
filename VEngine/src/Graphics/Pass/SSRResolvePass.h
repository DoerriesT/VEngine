#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SSRResolvePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
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

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}
