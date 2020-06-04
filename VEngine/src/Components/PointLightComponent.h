#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct PointLightComponent
	{
		glm::vec3 m_color;
		float m_luminousPower;
		float m_radius;
		bool m_shadows;
		bool m_volumetricShadows; // requires m_shadows to be true
	};
}