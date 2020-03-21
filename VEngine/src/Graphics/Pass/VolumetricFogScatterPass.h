#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;

	namespace VolumetricFogScatterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_frustumCorners[4][3];
			float m_jitter[3];
			float m_reprojectedTexCoordScaleBias[4];
			const DirectionalLightData *m_lightData;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_prevResultImageViewHandle;
			rg::ImageViewHandle m_scatteringExtinctionImageViewHandle;
			rg::ImageViewHandle m_emissivePhaseImageViewHandle;
			rg::ImageViewHandle m_shadowImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}