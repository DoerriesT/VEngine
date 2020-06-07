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
		bool m_heightFogEnabled;
		float m_heightFogStart;
		float m_heightFogFalloff;
		float m_maxHeight;
		float m_noiseIntensity;
		float m_noiseScale;
		glm::vec3 m_noiseBias;
	};

	struct LocalParticipatingMediumComponent
	{
		glm::vec3 m_albedo;
		float m_extinction;
		glm::vec3 m_emissiveColor;
		float m_emissiveIntensity;
		float m_phaseAnisotropy;
		float m_noiseIntensity;
		glm::vec3 m_noiseScale;
		glm::vec3 m_noiseBias;
	};
}