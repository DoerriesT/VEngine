#include "EntityManager.h"

VEngine::Entity::Entity(const Id &id, const Id &version)
	: m_id(id),
	m_version(version)
{
}

const VEngine::Entity *VEngine::EntityManager::createEntity()
{
	std::uint64_t id;
	if (m_freeIds.empty())
	{
		id = m_nextFreeId++;
	}
	else
	{
		id = m_freeIds.back();
		m_freeIds.pop_back();
	}
	Entity *entity = new Entity(id, m_entityIdVersionMap[id]);
	m_entityIdToComponentBitSetMap[id] = 0;
	m_entityIdToComponentMap[id].clear();

	//invoke listeners
	for (IOnEntityCreatedListener *listener : m_onEntityCreatedListeners)
	{
		listener->onEntityCreated(entity);
	}

	return entity;
}

void VEngine::EntityManager::removeComponent(const Entity *entity, uint64_t componentTypeId, bool notify)
{
	assert(validateEntity(entity));
	const Entity::Id &id = entity->m_id;

	// only do something if the component is actually attached to the entity
	if (m_entityIdToComponentBitSetMap[id][componentTypeId])
	{
		// fetch the component pointer
		BaseComponent *component = m_entityIdToComponentMap[id][componentTypeId];
		assert(component);

		// remove type and family bits from their map
		m_entityIdToComponentBitSetMap[id].reset(componentTypeId);

		// invoke listeners
		if (notify)
		{
			for (IOnComponentRemovedListener *listener : m_onComponentRemovedListeners)
			{
				listener->onComponentRemoved(entity, component);
			}
		}

		// delete pointer and remove component from maps
		delete component;
		ContainerUtility::remove(m_entityIdToComponentMap[id], componentTypeId);
	}
}

void VEngine::EntityManager::destroyEntity(const Entity *entity)
{
	assert(validateEntity(entity));

	const Entity::Id &id = entity->m_id;

	//invoke listeners
	for (IOnEntityDestructionListener *listener : m_onEntityDestructionListeners)
	{
		listener->onDestruction(entity);
	}

	for (uint64_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
	{
		removeComponent(entity, i, false);
	}

	// bitmaps should already be zero
	assert(m_entityIdToComponentBitSetMap[id] == 0);
	// reset bitmaps
	// entityIdToComponentBitFieldMap[id] = 0;
	// entityIdToFamilyBitFieldMap[id] = 0;
	// increase version
	++m_entityIdVersionMap[id];
	m_freeIds.push_back(id);

	delete entity;
}

std::bitset<COMPONENT_TYPE_COUNT> VEngine::EntityManager::getComponentBitSet(const Entity *entity)
{
	assert(validateEntity(entity));
	return m_entityIdToComponentBitSetMap[entity->m_id];
}

std::unordered_map<std::uint64_t, VEngine::BaseComponent *> VEngine::EntityManager::getComponentMap(const Entity *entity)
{
	assert(validateEntity(entity));
	return m_entityIdToComponentMap[entity->m_id];
}

void VEngine::EntityManager::addOnEntityCreatedListener(IOnEntityCreatedListener *listener)
{
	assert(listener);
	m_onEntityCreatedListeners.push_back(listener);
}

void VEngine::EntityManager::addOnEntityDestructionListener(IOnEntityDestructionListener *listener)
{
	assert(listener);
	m_onEntityDestructionListeners.push_back(listener);
}

void VEngine::EntityManager::addOnComponentAddedListener(IOnComponentAddedListener *listener)
{
	assert(listener);
	m_onComponentAddedListeners.push_back(listener);
}

void VEngine::EntityManager::addOnComponentRemovedListener(IOnComponentRemovedListener *listener)
{
	assert(listener);
	m_onComponentRemovedListeners.push_back(listener);
}

void VEngine::EntityManager::removeOnEntityCreatedListener(IOnEntityCreatedListener *listener)
{
	assert(listener);
	ContainerUtility::remove(m_onEntityCreatedListeners, listener);
}

void VEngine::EntityManager::removeOnEntityDestructionListener(IOnEntityDestructionListener *listener)
{
	assert(listener);
	ContainerUtility::remove(m_onEntityDestructionListeners, listener);
}

void VEngine::EntityManager::removeOnComponentAddedListener(IOnComponentAddedListener *listener)
{
	assert(listener);
	ContainerUtility::remove(m_onComponentAddedListeners, listener);
}

void VEngine::EntityManager::removeOnComponentRemovedListener(IOnComponentRemovedListener *listener)
{
	assert(listener);
	ContainerUtility::remove(m_onComponentRemovedListeners, listener);
}

bool VEngine::EntityManager::validateEntity(const Entity *entity)
{
	assert(entity);

	const Entity::Id &id = entity->m_id;
	const Entity::Version &version = entity->m_version;

	//check if given id is still a valid id and present in all maps
	return ContainerUtility::contains(m_entityIdVersionMap, id) &&
		m_entityIdVersionMap[id] == version &&
		ContainerUtility::contains(m_entityIdToComponentBitSetMap, id) &&
		ContainerUtility::contains(m_entityIdToComponentMap, id);
}
