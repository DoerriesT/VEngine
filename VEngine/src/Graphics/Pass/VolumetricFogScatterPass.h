#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;
	struct CommonRenderData;

	namespace VolumetricFogScatterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const DirectionalLightData *m_lightData;
			const CommonRenderData *m_commonData;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}