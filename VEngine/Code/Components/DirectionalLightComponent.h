#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct DirectionalLightComponent
	{
		glm::vec3 m_color;
		glm::vec3 m_direction;
		bool m_shadows;
	};
}