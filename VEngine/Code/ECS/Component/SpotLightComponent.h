#pragma once
#include "Component.h"
#include <glm/vec3.hpp>

namespace VEngine
{
	struct SpotLightComponent : public Component<SpotLightComponent>
	{
		glm::vec3 m_direction;
		glm::vec3 m_color;
		float m_luminousPower;
		float m_radius;
		float m_outerAngle;
		float m_innerAngle;

		explicit SpotLightComponent(glm::vec3 direction, glm::vec3 color, float luminousPower, float radius, float outerAngle, float innerAngle)
			:m_direction(direction),
			m_color(color),
			m_luminousPower(luminousPower),
			m_radius(radius),
			m_outerAngle(outerAngle),
			m_innerAngle(innerAngle)
		{
		}
	};
}