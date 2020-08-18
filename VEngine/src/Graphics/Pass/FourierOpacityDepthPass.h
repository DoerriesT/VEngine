#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct LightData;
	struct ParticleDrawData;

	namespace FourierOpacityDepthPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const LightData *m_lightData;
			uint32_t m_listCount;
			ParticleDrawData **m_particleLists;
			uint32_t *m_listSizes;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_particleBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatrixBufferInfo;
			rg::ImageHandle m_directionalLightFomDepthImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}