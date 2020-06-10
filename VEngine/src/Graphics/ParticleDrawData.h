#pragma once
#include <glm/vec3.hpp>

namespace VEngine
{
	struct ParticleDrawData
	{
		glm::vec3 m_position;
		float m_opacity;
		uint32_t m_textureIndex;
		float m_pad0;
		float m_pad1;
		float m_pad2;
	};
}