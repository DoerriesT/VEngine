#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct BoundingBoxComponent
	{
		glm::vec3 m_extent = glm::vec3(1.0f);
	};
}