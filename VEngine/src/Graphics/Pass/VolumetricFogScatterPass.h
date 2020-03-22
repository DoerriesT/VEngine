#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;

	namespace VolumetricFogScatterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_frustumCorners[4][3];
			float m_jitter[3];
			float m_reprojectedTexCoordScaleBias[4];
			const DirectionalLightData *m_lightData;
			gal::DescriptorBufferInfo m_pointLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightZBinsBufferInfo;
			gal::DescriptorBufferInfo m_spotLightDataBufferInfo;
			gal::DescriptorBufferInfo m_spotLightZBinsBufferInfo;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
			rg::BufferViewHandle m_spotLightBitMaskBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_prevResultImageViewHandle;
			rg::ImageViewHandle m_scatteringExtinctionImageViewHandle;
			rg::ImageViewHandle m_emissivePhaseImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}