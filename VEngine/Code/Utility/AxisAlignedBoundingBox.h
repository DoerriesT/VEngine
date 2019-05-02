#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct AxisAlignedBoundingBox
	{
		glm::vec3 m_min;
		glm::vec3 m_max;
	};
}