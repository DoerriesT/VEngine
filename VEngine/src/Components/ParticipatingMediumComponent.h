#pragma once
#include <glm/vec3.hpp>
#include "Handles.h"

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
		Texture3DHandle m_densityTexture;
		float m_textureScale;
		glm::vec3 m_textureBias;
	};

	struct LocalParticipatingMediumComponent
	{
		glm::vec3 m_albedo;
		float m_extinction;
		glm::vec3 m_emissiveColor;
		float m_emissiveIntensity;
		float m_phaseAnisotropy;
		bool m_heightFogEnabled;
		float m_heightFogStart;
		float m_heightFogFalloff;
		Texture3DHandle m_densityTexture;
		glm::vec3 m_textureScale;
		glm::vec3 m_textureBias;
	};
}