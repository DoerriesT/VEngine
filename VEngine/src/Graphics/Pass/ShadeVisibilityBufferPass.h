#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	namespace ShadeVisibilityBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			gal::DescriptorBufferInfo m_indexBufferInfo;
			rg::BufferViewHandle m_punctualLightsBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsShadowedBitMaskBufferHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_deferredShadowImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_normalRoughnessImageViewHandle;
			rg::ImageViewHandle m_albedoMetalnessImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::ImageViewHandle m_fomImageViewHandle;
			rg::ImageViewHandle m_triangleImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}