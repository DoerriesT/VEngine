#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLight;

	namespace VolumetricFogScatterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_frustumCorners[4][3];
			float m_jitter[6];
			bool m_checkerboard;
			bool m_ignoreHistory;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_localMediaZBinsBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			rg::ImageViewHandle m_localMediaBitMaskImageViewHandle;
			rg::ImageViewHandle m_punctualLightsBitMaskImageViewHandle;
			rg::ImageViewHandle m_punctualLightsShadowedBitMaskImageViewHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::ImageViewHandle m_fomImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMDepthRangeImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}