#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLight;

	namespace DeferredShadowsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_lightDataCount;
			const DirectionalLight *m_lightData;
			gal::ImageView *m_blueNoiseImageView;
			rg::ImageHandle m_resultImageHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			//rg::ImageViewHandle m_tangentSpaceImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMImageViewHandle;
			rg::ImageViewHandle m_directionalLightFOMDepthRangeImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			gal::DescriptorBufferInfo m_cascadeParamsBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}