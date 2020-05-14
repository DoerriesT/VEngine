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
			uint32_t m_probeIndex;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedProbeBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			rg::ImageViewHandle m_directionalShadowImageViewHandle;
			gal::ImageView *m_depthImageView; // all layers
			gal::ImageView *m_albedoRoughnessImageView; // all layers
			gal::ImageView *m_normalImageView; // all layers
			rg::ImageViewHandle m_resultImageViewHandle; // 6 layers
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}