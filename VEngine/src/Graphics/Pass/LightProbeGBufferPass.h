#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/vec3.hpp>

namespace VEngine
{
	struct PassRecordContext;

	namespace LightProbeGBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			glm::vec3 m_probePosition;
			float m_probeNearPlane;
			float m_probeFarPlane;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedProbeBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			rg::ImageViewHandle m_directionalShadowImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle; // 6 layers
			rg::ImageViewHandle m_albedoRoughnessImageViewHandle; // 6 layers
			rg::ImageViewHandle m_normalImageViewHandle; // 6 layers
			rg::ImageViewHandle m_resultImageViewHandle; // 6 layers
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}