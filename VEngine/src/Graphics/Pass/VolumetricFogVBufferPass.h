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
			float m_jitterX;
			float m_jitterY;
			float m_jitterZ;
			rg::ImageViewHandle m_scatteringExtinctionImageViewHandle;
			rg::ImageViewHandle m_emissivePhaseImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}