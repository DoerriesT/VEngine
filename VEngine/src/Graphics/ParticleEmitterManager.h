#pragma once
#include <entt/entity/registry.hpp>
#include <random>
#include <vector>
#include <glm/mat4x4.hpp>
#include "ParticleDrawData.h"

namespace VEngine
{
	class ParticleEmitterManager
	{
	public:
		explicit ParticleEmitterManager(entt::registry &entityRegistry);
		void update(float timeDelta, const glm::mat4 viewMatrix);
		void getParticleDrawData(uint32_t &listCount, ParticleDrawData **&particleLists, uint32_t *&listSizes);

	private:
		entt::registry &m_entityRegistry;
		std::default_random_engine m_randomEngine;
		float m_time;
		std::vector<ParticleDrawData *> m_particleDrawDataLists;
		std::vector<uint32_t> m_particleDrawDataListSizes;

		void addInternalComponent(entt::registry &registry, entt::entity entity);
		void removeInternalComponent(entt::registry &registry, entt::entity entity);
	};
}