#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct LocalReflectionProbeComponent
	{
		glm::vec3 m_captureOffset;
		float m_transitionDistance;
	};
}