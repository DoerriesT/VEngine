#pragma once
#include <cstdint>
#include <glm/vec3.hpp>
#include "Handles.h"

namespace VEngine
{
	struct ParticleEmitterComponent
	{
		enum SpawnType
		{
			SPHERE, CUBE, DISK
		};
		glm::vec3 m_direction = glm::vec3(0.0f, 5.0f, 0.0f);
		uint32_t m_particleCount = 8;
		float m_particleLifetime = 10.0f;
		float m_velocityMagnitude = 1.0f;
		SpawnType m_spawnType = SpawnType::DISK;
		float m_spawnAreaSize = 0.1f;
		float m_spawnRate = 2.0f;
		float m_particleSize = 0.5f;
		float m_particleFinalSize = 1.0f;
		float m_rotation = 0.0f;
		float m_FOMOpacityMult = 0.5f;
		Texture2DHandle m_textureHandle = {};
	};
}