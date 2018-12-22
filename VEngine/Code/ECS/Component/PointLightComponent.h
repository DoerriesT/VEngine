#pragma once
#include "Component.h"
#include <glm/vec3.hpp>

namespace VEngine
{
	struct PointLightComponent : public Component<PointLightComponent>
	{
		glm::vec3 m_color;
		float m_luminousPower;
		float m_radius;

		explicit PointLightComponent(const glm::vec3 &color, float luminousPower, float radius)
			:m_color(color),
			m_luminousPower(luminousPower),
			m_radius(radius)
		{
		}
	};
}