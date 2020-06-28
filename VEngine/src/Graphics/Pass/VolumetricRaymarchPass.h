#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricRaymarchPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::ImageView *m_blueNoiseImageView;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}