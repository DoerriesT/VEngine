#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ApplyIndirectLightingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ssao;
			gal::DescriptorBufferInfo m_reflectionProbeDataBufferInfo;
			gal::DescriptorBufferInfo m_reflectionProbeZBinsBufferInfo;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			gal::ImageView *m_reflectionProbeImageView;
			rg::ImageViewHandle m_reflectionProbeBitMaskImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle2;
			//rg::ImageViewHandle m_indirectSpecularLightImageViewHandle;
			rg::ImageViewHandle m_brdfLutImageViewHandle;
			rg::ImageViewHandle m_albedoMetalnessImageViewHandle;
			rg::ImageViewHandle m_normalRoughnessImageViewHandle;
			rg::ImageViewHandle m_ssaoImageViewHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}