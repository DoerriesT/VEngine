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
		float m_initialSize;
		float m_finalSize;
		float m_rotation;
		float m_angularVelocity;
	};

	struct ParticleEmitterInternalDataComponent
	{
		ParticleState *m_particleData; // owns the memory
		ParticleState *m_prevParticles; // offset into m_particleData
		ParticleState *m_curParticles; // offset into m_particleData
		uint32_t m_activeParticleCount;
		uint32_t m_totalParticleCount;
		float m_lastEmissionTime;
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

	auto view = m_entityRegistry.view<TransformationComponent, ParticleEmitterComponent, ParticleEmitterInternalDataComponent>();

	view.each([&](entt::entity entity, TransformationComponent &transformationComponent, ParticleEmitterComponent &emitterComponent, ParticleEmitterInternalDataComponent &internalComponent)
		{
			// particle count was changed
			if (emitterComponent.m_particleCount != internalComponent.m_totalParticleCount)
			{
				// delete memory and exit early
				if (emitterComponent.m_particleCount == 0)
				{
					if (internalComponent.m_totalParticleCount > 0)
					{
						delete[] internalComponent.m_particleData;
						delete[] internalComponent.m_drawData;
					}

					internalComponent = {};

					return;
				}

				ParticleState *newParticleStateMemory = new ParticleState[emitterComponent.m_particleCount * 2];
				ParticleDrawData *newParticleDrawData = new ParticleDrawData[emitterComponent.m_particleCount];

				// try to copy as much as possible of the old state to the new memory
				if (internalComponent.m_totalParticleCount > 0)
				{
					memcpy(newParticleStateMemory, internalComponent.m_particleData, sizeof(ParticleState) * emitterComponent.m_particleCount);
					memcpy(newParticleStateMemory + emitterComponent.m_particleCount, internalComponent.m_particleData + internalComponent.m_totalParticleCount, sizeof(ParticleState) * emitterComponent.m_particleCount);
				}
			
				// create new pointers to double buffer halfs
				ParticleState *prevParticles = (internalComponent.m_particleData && internalComponent.m_prevParticles == internalComponent.m_particleData) ? newParticleStateMemory : newParticleStateMemory + emitterComponent.m_particleCount;
				ParticleState *curParticles = (prevParticles == newParticleStateMemory) ? newParticleStateMemory + emitterComponent.m_particleCount : newParticleStateMemory;

				// free old memory
				if (internalComponent.m_totalParticleCount > 0)
				{
					delete[] internalComponent.m_particleData;
					delete[] internalComponent.m_drawData;
				}

				// update internal data
				internalComponent.m_particleData = newParticleStateMemory;
				internalComponent.m_prevParticles = prevParticles;
				internalComponent.m_curParticles = curParticles;
				internalComponent.m_activeParticleCount = glm::min(internalComponent.m_activeParticleCount, emitterComponent.m_particleCount);
				internalComponent.m_totalParticleCount = emitterComponent.m_particleCount;
				internalComponent.m_drawData = newParticleDrawData;
			}

			// early exit when emitter has 0 particles: the particle pointers are null
			if (emitterComponent.m_particleCount == 0)
			{
				return;
			}

			// swap buffers
			std::swap(internalComponent.m_curParticles, internalComponent.m_prevParticles);

			const float particleLifetime = glm::max(0.0001f, emitterComponent.m_particleLifetime);
			uint32_t particleCount = 0;

			// simulate particles
			for (size_t i = 0; i < internalComponent.m_activeParticleCount; ++i)
			{
				auto &particle = internalComponent.m_prevParticles[i];
				if ((m_time - particle.m_timeOfBirth) < particle.m_lifeTime)
				{
					particle.m_position += particle.m_velocity * timeDelta;
					particle.m_velocity.y -= 0.1f * timeDelta / particleLifetime;
					particle.m_rotation += particle.m_angularVelocity * timeDelta;

					const float lifePercentage = (m_time - particle.m_timeOfBirth) / particle.m_lifeTime;

					assert(particleCount < emitterComponent.m_particleCount);
					auto &curBufferParticle = internalComponent.m_curParticles[particleCount++];
					curBufferParticle = particle;

					auto &drawData = internalComponent.m_drawData[particleCount - 1];
					drawData = {};
					drawData.m_position = particle.m_position;
					drawData.m_opacity = lifePercentage > 0.9f ? 10.0f * (1.0f - lifePercentage) : 1.0f;
					drawData.m_textureIndex = emitterComponent.m_textureHandle.m_handle;
					drawData.m_size = glm::mix(particle.m_initialSize, particle.m_finalSize, lifePercentage);
					drawData.m_rotation = particle.m_rotation;
					drawData.m_FOMOpacityMult = emitterComponent.m_FOMOpacityMult;
				}
			}

			// spawn new particles
			const uint32_t deadParticleCount = emitterComponent.m_particleCount - particleCount;
			const float emissionRate = emitterComponent.m_particleCount / particleLifetime;
			float timeSinceLastEmission = m_time - internalComponent.m_lastEmissionTime;
			const uint32_t particlesToEmit = glm::min(deadParticleCount, static_cast<uint32_t>(timeSinceLastEmission * emitterComponent.m_spawnRate));
			if (particlesToEmit > 0)
			{
				internalComponent.m_lastEmissionTime = m_time;
			}

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
				particle.m_lifeTime = uniformD(m_randomEngine) * particleLifetime * 0.5f + particleLifetime * 0.5f;
				particle.m_timeOfBirth = m_time;

				// particle size
				particle.m_initialSize = emitterComponent.m_particleSize;
				particle.m_finalSize = emitterComponent.m_particleFinalSize;

				// rotation
				particle.m_rotation = uniformD(m_randomEngine) * glm::pi<float>() * 2.0f;
				particle.m_angularVelocity = (uniformD(m_randomEngine) > 0.5f ? 1.0f : -1.0f) * uniformD(m_randomEngine) * emitterComponent.m_rotation;
				

				assert(particleCount < emitterComponent.m_particleCount);
				internalComponent.m_curParticles[particleCount++] = particle;

				auto &drawData = internalComponent.m_drawData[particleCount - 1];
				drawData = {};
				drawData.m_position = particle.m_position;
				drawData.m_opacity = 1.0f;
				drawData.m_textureIndex = emitterComponent.m_textureHandle.m_handle;
				drawData.m_size = particle.m_initialSize;
				drawData.m_rotation = particle.m_rotation;
				drawData.m_FOMOpacityMult = emitterComponent.m_FOMOpacityMult;
			}

			internalComponent.m_activeParticleCount = particleCount;

			glm::vec4 viewMatrixRow2 = glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], viewMatrix[3][2]);
			std::sort(internalComponent.m_drawData, internalComponent.m_drawData + particleCount, [&](const auto &lhs, const auto &rhs)
				{
					return glm::dot(glm::vec4(lhs.m_position, 1.0f), viewMatrixRow2) < glm::dot(glm::vec4(rhs.m_position, 1.0f), viewMatrixRow2);
				});

			if (m_entityRegistry.has<RenderableComponent>(entity))
			{
				m_particleDrawDataLists.push_back(internalComponent.m_drawData);
				m_particleDrawDataListSizes.push_back(internalComponent.m_activeParticleCount);
			}
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
	auto &emitterC = registry.get<ParticleEmitterComponent>(entity);
	//assert(emitterC.m_particleCount > 0);

	auto &internalC = registry.assign<ParticleEmitterInternalDataComponent>(entity);
	internalC = {};
	if (emitterC.m_particleCount > 0)
	{
		internalC.m_particleData = new ParticleState[emitterC.m_particleCount * 2];
		internalC.m_prevParticles = internalC.m_particleData;
		internalC.m_curParticles = internalC.m_particleData + emitterC.m_particleCount;
		internalC.m_activeParticleCount = 0;
		internalC.m_totalParticleCount = emitterC.m_particleCount;
		internalC.m_lastEmissionTime = 0.0f;
		internalC.m_drawData = new ParticleDrawData[emitterC.m_particleCount];
	}
}

void VEngine::ParticleEmitterManager::removeInternalComponent(entt::registry &registry, entt::entity entity)
{
	const auto &internalC = registry.get<ParticleEmitterInternalDataComponent>(entity);
	if (internalC.m_totalParticleCount > 0)
	{
		delete[] internalC.m_particleData;
		delete[] internalC.m_drawData;
	}
	registry.remove<ParticleEmitterInternalDataComponent>(entity);
}
