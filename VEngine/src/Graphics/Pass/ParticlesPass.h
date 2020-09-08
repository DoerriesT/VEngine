#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct ParticleDrawData;

	namespace ParticlesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_listCount;
			ParticleDrawData **m_particleLists;
			uint32_t *m_listSizes;
			gal::ImageView *m_blueNoiseImageView;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_particleBufferInfo;
			rg::ImageViewHandle m_punctualLightsBitMaskImageViewHandle;
			rg::ImageViewHandle m_punctualLightsShadowedBitMaskImageViewHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::ImageViewHandle m_fomImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMDepthRangeImageViewHandle;
			rg::ImageViewHandle m_volumetricFogImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}