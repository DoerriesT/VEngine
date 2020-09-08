#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct LocalReflectionProbeComponent
	{
		glm::vec3 m_captureOffset = glm::vec3(0.0f);
		float m_transitionDistance = 0.0f;
		bool m_recapture;
	};
}