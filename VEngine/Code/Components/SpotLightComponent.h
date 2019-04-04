#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct SpotLightComponent
	{
		glm::vec3 m_direction;
		glm::vec3 m_color;
		float m_luminousPower;
		float m_radius;
		float m_outerAngle;
		float m_innerAngle;
	};
}