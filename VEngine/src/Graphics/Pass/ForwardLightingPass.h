#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	namespace ForwardLightingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_instanceDataCount;
			uint32_t m_instanceDataOffset;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			const float *m_texCoordScaleBias;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			//gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			//gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			rg::ImageViewHandle m_punctualLightsBitMaskImageViewHandle;
			rg::ImageViewHandle m_punctualLightsShadowedBitMaskImageViewHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			//rg::BufferViewHandle m_indicesBufferHandle;
			//rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_deferredShadowImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_normalRoughnessImageViewHandle;
			rg::ImageViewHandle m_albedoMetalnessImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			gal::ImageView *m_probeImageView;
			rg::ImageViewHandle m_fomImageViewHandle;
			gal::DescriptorBufferInfo m_atmosphereConstantBufferInfo;
			rg::ImageViewHandle m_atmosphereScatteringImageViewHandle;
			rg::ImageViewHandle m_atmosphereTransmittanceImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}