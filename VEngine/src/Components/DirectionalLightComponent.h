#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct DirectionalLightComponent
	{
		enum {MAX_CASCADES = 8};
		glm::vec3 m_color = glm::vec3(1.0f);
		float m_intensity = 100.0f;
		bool m_shadows = false;
		uint32_t m_cascadeCount = 4;
		float m_maxShadowDistance = 100.0f;
		float m_splitLambda = 0.9f;
		float m_depthBias[MAX_CASCADES] = {};
		float m_normalOffsetBias[MAX_CASCADES] = {};
	};
}