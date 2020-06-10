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
		glm::vec3 m_direction;
		uint32_t m_particleCount;
		float m_particleLifetime;
		float m_velocityMagnitude;
		SpawnType m_spawnType;
		float m_spawnAreaSize;
	};
}