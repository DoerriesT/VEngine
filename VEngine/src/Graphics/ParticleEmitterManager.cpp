#include "ParticleEmitterManager.h"
#include "Components/TransformationComponent.h"
#include "Components/ParticleEmitterComponent.h"
#include "Components/RenderableComponent.h"
#include <glm/vec3.hpp>

namespace
{
	struct ParticleState
	{
		glm::vec3 m_position;
		float m_timeOfBirth;
		glm::vec3 m_velocity;
		float m_lifeTime;
	};

	struct ParticleEmitterInternalDataComponent
	{
		ParticleState *m_particleData; // owns the memory
		ParticleState *m_prevParticles; // offset into m_particleData
		ParticleState *m_curParticles; // offset into m_particleData
		uint32_t m_activeParticleCount;
		VEngine::ParticleDrawData *m_drawData; // owns memory
	};
}


VEngine::ParticleEmitterManager::ParticleEmitterManager(entt::registry &entityRegistry)
	:m_entityRegistry(entityRegistry),
	m_randomEngine(),
	m_time()
{
	m_entityRegistry.on_assign<ParticleEmitterComponent>().connect<&ParticleEmitterManager::addInternalComponent>(this);
	m_entityRegistry.on_remove<ParticleEmitterComponent>().connect<&ParticleEmitterManager::removeInternalComponent>(this);
}

void VEngine::ParticleEmitterManager::update(float timeDelta, const glm::mat4 viewMatrix)
{
	m_time += timeDelta;
	m_particleDrawDataLists.clear();
	m_particleDrawDataListSizes.clear();

	auto view = m_entityRegistry.view<TransformationComponent, ParticleEmitterComponent, ParticleEmitterInternalDataComponent, RenderableComponent>();

	view.each([&](entt::entity entity, TransformationComponent &transformationComponent, ParticleEmitterComponent &emitterComponent, ParticleEmitterInternalDataComponent &internalComponent, RenderableComponent &)
		{
			// swap buffers
			std::swap(internalComponent.m_curParticles, internalComponent.m_prevParticles);

			uint32_t particleCount = 0;

			// simulate particles
			for (size_t i = 0; i < internalComponent.m_activeParticleCount; ++i)
			{
				auto &particle = internalComponent.m_prevParticles[i];
				if ((m_time - particle.m_timeOfBirth) < particle.m_lifeTime)
				{
					particle.m_position += particle.m_velocity * timeDelta;
					particle.m_velocity.y -= 0.1f * timeDelta / emitterComponent.m_particleLifetime;

					const float lifePercentage = (m_time - particle.m_timeOfBirth) / particle.m_lifeTime;
					const float opacity = lifePercentage > 0.9f ? 10.0f * (1.0f - lifePercentage) : 1.0f;

					assert(particleCount < emitterComponent.m_particleCount);
					auto &curBufferParticle = internalComponent.m_curParticles[particleCount++];
					curBufferParticle = particle;

					auto &drawData = internalComponent.m_drawData[particleCount - 1];
					drawData = {};
					drawData.m_position = particle.m_position;
					drawData.m_opacity = opacity;
					drawData.m_textureIndex = emitterComponent.m_textureHandle.m_handle;
				}
			}

			// spawn new particles
			const uint32_t deadParticleCount = emitterComponent.m_particleCount - particleCount;
			const float emissionRate = emitterComponent.m_particleCount / emitterComponent.m_particleLifetime;
			const uint32_t particlesToEmit = deadParticleCount;// glm::min(deadParticleCount, static_cast<uint32_t>(emissionRate * timeDelta));

			std::normal_distribution<float> sphereNormalD(0.5f, 1.0f);
			std::uniform_real_distribution<float> cubeD(-emitterComponent.m_spawnAreaSize, emitterComponent.m_spawnAreaSize);
			std::uniform_real_distribution<float> uniformD(0.0f, 1.0f);
			std::uniform_real_distribution<float> angleD(0.0f, 2.0f * glm::pi<float>());
			std::uniform_real_distribution<float> radiusD(0.0f, emitterComponent.m_spawnAreaSize);
			std::uniform_real_distribution<float> velMagD(0.5f, 1.0f);

			for (size_t i = 0; i < particlesToEmit; ++i)
			{
				ParticleState particle = {};

				// determine initial particle position
				switch (emitterComponent.m_spawnType)
				{
				case ParticleEmitterComponent::SPHERE:
					particle.m_position = glm::normalize(glm::vec3(sphereNormalD(m_randomEngine), sphereNormalD(m_randomEngine), sphereNormalD(m_randomEngine)));
					particle.m_position *= glm::pow(uniformD(m_randomEngine), 1.0f / 3.0f);
					break;
				case ParticleEmitterComponent::CUBE:
				{
					particle.m_position = glm::vec3(cubeD(m_randomEngine), cubeD(m_randomEngine), cubeD(m_randomEngine));
					break;
				}
				case ParticleEmitterComponent::DISK:
				{
					const float angle = angleD(m_randomEngine);
					const float radius = radiusD(m_randomEngine);
					particle.m_position = glm::vec3(glm::cos(angle), 0.0f, glm::sin(angle)) * radius;
					break;
				}
				default:
					assert(false);
					break;
				}

				// determine particle velocity
				particle.m_velocity = glm::normalize(glm::vec3(sphereNormalD(m_randomEngine), sphereNormalD(m_randomEngine), sphereNormalD(m_randomEngine)));
				particle.m_velocity *= glm::pow(uniformD(m_randomEngine), 1.0f / 3.0f);
				particle.m_velocity += emitterComponent.m_direction;
				particle.m_velocity = glm::normalize(particle.m_velocity);
				particle.m_velocity *= velMagD(m_randomEngine) * emitterComponent.m_velocityMagnitude;

				glm::mat3 rotation = glm::mat3_cast(transformationComponent.m_orientation);

				// rotate and translate particle
				particle.m_position = transformationComponent.m_position + (rotation * particle.m_position);
				particle.m_velocity = rotation * particle.m_velocity;

				// determine particle lifetime
				particle.m_lifeTime = uniformD(m_randomEngine) * emitterComponent.m_particleLifetime;
				particle.m_timeOfBirth = m_time;

				assert(particleCount < emitterComponent.m_particleCount);
				internalComponent.m_curParticles[particleCount++] = particle;

				auto &drawData = internalComponent.m_drawData[particleCount - 1];
				drawData = {};
				drawData.m_position = particle.m_position;
				drawData.m_opacity = 1.0f;
				drawData.m_textureIndex = emitterComponent.m_textureHandle.m_handle;
			}

			internalComponent.m_activeParticleCount = particleCount;

			glm::vec4 viewMatrixRow2 = glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], viewMatrix[3][2]);
			std::sort(internalComponent.m_drawData, internalComponent.m_drawData + particleCount, [&](const auto &lhs, const auto &rhs)
				{
					return glm::dot(glm::vec4(lhs.m_position, 1.0f), viewMatrixRow2) < glm::dot(glm::vec4(rhs.m_position, 1.0f), viewMatrixRow2);
				});

			m_particleDrawDataLists.push_back(internalComponent.m_drawData);
			m_particleDrawDataListSizes.push_back(internalComponent.m_activeParticleCount);
		});
}

void VEngine::ParticleEmitterManager::getParticleDrawData(uint32_t &listCount, ParticleDrawData **&particleLists, uint32_t *&listSizes)
{
	listCount = static_cast<uint32_t>(m_particleDrawDataLists.size());
	particleLists = m_particleDrawDataLists.data();
	listSizes = m_particleDrawDataListSizes.data();
}

void VEngine::ParticleEmitterManager::addInternalComponent(entt::registry &registry, entt::entity entity)
{
	const auto &emitterC = registry.get<ParticleEmitterComponent>(entity);
	assert(emitterC.m_particleCount > 0);

	auto &internalC = registry.assign<ParticleEmitterInternalDataComponent>(entity);
	internalC.m_particleData = new ParticleState[emitterC.m_particleCount * 2];
	internalC.m_prevParticles = internalC.m_particleData;
	internalC.m_curParticles = internalC.m_particleData + emitterC.m_particleCount;
	internalC.m_activeParticleCount = 0;
	internalC.m_drawData = new ParticleDrawData[emitterC.m_particleCount];
}

void VEngine::ParticleEmitterManager::removeInternalComponent(entt::registry &registry, entt::entity entity)
{
	const auto &internalC = registry.get<ParticleEmitterInternalDataComponent>(entity);
	delete[] internalC.m_particleData;
	delete[] internalC.m_drawData;
	registry.remove<ParticleEmitterInternalDataComponent>(entity);
}
