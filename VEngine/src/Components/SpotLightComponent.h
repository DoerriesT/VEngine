#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct SpotLightComponent
	{
		glm::vec3 m_color = glm::vec3(1.0f);
		float m_luminousPower = 1000.0f;
		float m_radius = 8.0f;
		float m_outerAngle = 0.785f;
		float m_innerAngle = 0.26f;
		bool m_shadows = false;
		bool m_volumetricShadows = false; // requires m_shadows to be true
	};
}