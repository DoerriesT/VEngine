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
			bool m_ignoreHistory;
			float m_bias;
			uint32_t m_noiseTextureHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_rayHitPDFImageHandle;
			rg::ImageViewHandle m_maskImageHandle;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_normalRoughnessImageHandle;
			rg::ImageViewHandle m_albedoMetalnessImageHandle;
			rg::ImageViewHandle m_prevColorImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
			rg::ImageViewHandle m_resultMaskImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}
