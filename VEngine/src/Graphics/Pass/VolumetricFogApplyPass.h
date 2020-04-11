#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogApplyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_volumetricFogImageViewHandle;
			rg::ImageViewHandle m_indirectSpecularLightImageViewHandle;
			rg::ImageViewHandle m_brdfLutImageViewHandle;
			rg::ImageViewHandle m_specularRoughnessImageViewHandle;
			rg::ImageViewHandle m_normalImageViewHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}