#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;
	struct DirectionalLightData;

	namespace DeferredShadowsPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			uint32_t m_lightDataCount;
			const DirectionalLightData *m_lightData;
			rg::ImageHandle m_resultImageHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_tangentSpaceImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			gal::DescriptorBufferInfo m_cascadeParamsBufferInfo;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}