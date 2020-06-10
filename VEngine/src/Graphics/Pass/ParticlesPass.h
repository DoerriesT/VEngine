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
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			rg::BufferViewHandle m_punctualLightsBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsShadowedBitMaskBufferHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::ImageViewHandle m_fomImageViewHandle;
			rg::ImageViewHandle m_volumetricFogImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}