#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct LocalReflectionProbeComponent
	{
		glm::vec3 m_captureOffset = glm::vec3(0.0f);
		float m_boxFadeDistances[6] = {}; // x, -x, y, -y, z, -z
		bool m_recapture = false;
		bool m_lockedFadeDistance = false;
	};
}