#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLight;

	namespace VolumetricFogMergedPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ignoreHistory;
			float m_frustumCorners[4][3];
			float m_jitter[6];
			float m_reprojectedTexCoordScaleBias[4];
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_localMediaZBinsBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			rg::BufferViewHandle m_localMediaBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsShadowedBitMaskBufferHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::ImageViewHandle m_fomImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}