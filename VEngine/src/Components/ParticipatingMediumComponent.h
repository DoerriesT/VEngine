#pragma once
#include <glm/vec3.hpp>
#include "Handles.h"

namespace VEngine
{
	struct GlobalParticipatingMediumComponent
	{
		glm::vec3 m_albedo = glm::vec3(1.0f);
		float m_extinction = 0.01f;
		glm::vec3 m_emissiveColor = glm::vec3(1.0f);
		float m_emissiveIntensity = 0.0f;
		float m_phaseAnisotropy = 0.0f;
		bool m_heightFogEnabled = true;
		float m_heightFogStart = 0.0f;
		float m_heightFogFalloff = 0.1f;
		float m_maxHeight = 0.0f;
		Texture3DHandle m_densityTexture = {};
		float m_textureScale = 1.0f;
		glm::vec3 m_textureBias = glm::vec3(0.0f);
	};

	struct LocalParticipatingMediumComponent
	{
		glm::vec3 m_albedo = glm::vec3(1.0f);
		float m_extinction = 0.01f;
		glm::vec3 m_emissiveColor = glm::vec3(1.0f);
		float m_emissiveIntensity = 0.0f;
		float m_phaseAnisotropy = 0.0f;
		bool m_heightFogEnabled = false;
		float m_heightFogStart = 0.0f;
		float m_heightFogFalloff = 0.1f;
		Texture3DHandle m_densityTexture = {};
		glm::vec3 m_textureScale = glm::vec3(1.0f);
		glm::vec3 m_textureBias = glm::vec3(0.0f);
	};
}