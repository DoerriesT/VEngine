#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct GlobalParticipatingMediumComponent
	{
		glm::vec3 m_albedo;
		float m_extinction;
		glm::vec3 m_emissiveColor;
		float m_emissiveIntensity;
		float m_phaseAnisotropy;
	};

	struct LocalParticipatingMediumComponent
	{
		glm::vec3 m_albedo;
		float m_extinction;
		glm::vec3 m_emissiveColor;
		float m_emissiveIntensity;
		float m_phaseAnisotropy;
	};
}