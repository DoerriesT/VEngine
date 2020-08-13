#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLight;
	struct ParticleDrawData;

	namespace FourierOpacityDirectionalLightPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_lightDataCount;
			const DirectionalLight *m_lightData;
			uint32_t m_listCount;
			ParticleDrawData **m_particleLists;
			uint32_t *m_listSizes;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_particleBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatrixBufferInfo;
			rg::ImageHandle m_directionalLightFomImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}