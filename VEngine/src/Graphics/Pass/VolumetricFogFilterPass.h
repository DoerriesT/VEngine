#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogFilterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_frustumCorners[4][3];
			float m_reprojectedTexCoordScaleBias[4];
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_inputImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}