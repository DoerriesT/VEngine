#include "RenderSystem.h"
#include "ECS/EntityManager.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include "ECS/Component/TransformationComponent.h"
#include "ECS/Component/ModelComponent.h"
#include "ECS/Component/RenderableComponent.h"

extern VEngine::GlobalVar<unsigned int> g_windowWidth;
extern VEngine::GlobalVar<unsigned int> g_windowHeight;

VEngine::RenderSystem::RenderSystem(EntityManager &entityManager)
	:m_entityManager(entityManager),
	m_renderer(std::make_unique<VKRenderer>())
{
	std::bitset<COMPONENT_TYPE_COUNT> validBitSet;
	validBitSet.set(TransformationComponent::getTypeId());
	validBitSet.set(ModelComponent::getTypeId());
	validBitSet.set(RenderableComponent::getTypeId());
	m_validBitSets.push_back(validBitSet);
}

VEngine::RenderSystem::~RenderSystem()
{
}

void VEngine::RenderSystem::init()
{
	m_renderer->init(g_windowWidth, g_windowHeight);
}

void VEngine::RenderSystem::input(double currentTime, double timeDelta)
{
}

void VEngine::RenderSystem::update(double currentTime, double timeDelta)
{
}

void VEngine::RenderSystem::render()
{
	m_renderer->update(nullptr);
	m_renderer->render();
}

void VEngine::RenderSystem::onComponentAdded(const Entity *entity, BaseComponent *addedComponent)
{
}

void VEngine::RenderSystem::onComponentRemoved(const Entity *entity, BaseComponent *removedComponent)
{
}

void VEngine::RenderSystem::onDestruction(const Entity *entity)
{
}
