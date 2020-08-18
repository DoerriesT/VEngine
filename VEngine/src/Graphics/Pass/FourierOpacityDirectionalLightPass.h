#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct LightData;
	struct ParticleDrawData;

	namespace FourierOpacityDirectionalLightPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const LightData *m_lightData;
			const glm::mat4 *m_shadowMatrices;
			uint32_t m_listCount;
			ParticleDrawData **m_particleLists;
			uint32_t *m_listSizes;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_particleBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatrixBufferInfo;
			rg::ImageViewHandle m_fomDepthRangeImageViewHandle;
			rg::ImageHandle m_directionalLightFomImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}