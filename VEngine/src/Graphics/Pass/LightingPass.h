#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LightingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::DescriptorBufferInfo m_directionalLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightZBinsBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_uvImageHandle;
			rg::ImageViewHandle m_ddxyLengthImageHandle;
			rg::ImageViewHandle m_ddxyRotMaterialIdImageHandle;
			rg::ImageViewHandle m_tangentSpaceImageHandle;
			rg::ImageViewHandle m_deferredShadowImageViewHandle;
			rg::ImageViewHandle m_resultImageHandle;
			rg::ImageViewHandle m_albedoImageHandle;
			rg::ImageViewHandle m_normalImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}