#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;
	struct CommonRenderData;

	namespace VolumetricFogVBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			float m_frustumCorners[4][3];
			float m_jitter[3];
			rg::ImageViewHandle m_scatteringExtinctionImageViewHandle;
			rg::ImageViewHandle m_emissivePhaseImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}