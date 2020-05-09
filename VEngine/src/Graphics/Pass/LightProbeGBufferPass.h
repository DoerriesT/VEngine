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
			rg::ImageViewHandle m_depthImageHandles[6];
			rg::ImageViewHandle m_albedoRoughnessImageHandles[6];
			rg::ImageViewHandle m_normalImageHandles[6];
			rg::ImageViewHandle m_resultImageHandles[6];
			rg::ImageViewHandle m_directionalShadowImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}