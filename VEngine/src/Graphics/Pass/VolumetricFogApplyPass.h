#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogApplyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ssao;
			uint32_t m_noiseTextureHandle;
			gal::DescriptorBufferInfo m_reflectionProbeDataBufferInfo;
			gal::DescriptorBufferInfo m_reflectionProbeZBinsBufferInfo;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			gal::ImageView *m_reflectionProbeImageView;
			rg::BufferViewHandle m_reflectionProbeBitMaskBufferHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_volumetricFogImageViewHandle;
			rg::ImageViewHandle m_indirectSpecularLightImageViewHandle;
			rg::ImageViewHandle m_brdfLutImageViewHandle;
			rg::ImageViewHandle m_albedoMetalnessImageViewHandle;
			rg::ImageViewHandle m_normalRoughnessImageViewHandle;
			rg::ImageViewHandle m_ssaoImageViewHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}