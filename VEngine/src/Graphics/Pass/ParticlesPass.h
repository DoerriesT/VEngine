#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct ParticleDrawData;

	namespace ParticlesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_listCount;
			ParticleDrawData **m_particleLists;
			uint32_t *m_listSizes;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}