#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace ForwardLightingPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			gal::DescriptorBufferInfo m_directionalLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightZBinsBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
			rg::BufferViewHandle m_indicesBufferHandle;
			rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_deferredShadowImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_normalImageViewHandle;
			rg::ImageViewHandle m_specularRoughnessImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}