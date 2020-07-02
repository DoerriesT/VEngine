#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogApplyPass2
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::ImageView *m_blueNoiseImageView;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_volumetricFogImageViewHandle;
			rg::ImageViewHandle m_raymarchedVolumetricsImageViewHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}