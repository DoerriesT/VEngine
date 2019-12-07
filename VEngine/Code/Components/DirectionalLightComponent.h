#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct DirectionalLightComponent
	{
		enum {MAX_CASCADES = 8};
		glm::vec3 m_color;
		float m_intensity;
		bool m_shadows;
		uint32_t m_cascadeCount;
		float m_maxShadowDistance;
		float m_splitLambda;
	};
}