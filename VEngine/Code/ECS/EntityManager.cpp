#include "EntityManager.h"
#include "Utility/ContainerUtility.h"

const VEngine::Entity *VEngine::EntityManager::createEntity(bool notifyListeners)
{
	Entity::ID id;
	if (m_freeEntityIds.empty())
	{
		id = m_nextFreeEntityId++;
	}
	else
	{
		id = m_freeEntityIds.back();
		m_freeEntityIds.pop_back();
	}

	Entity::Index index;
	if (m_freeComponentIndices.empty())
	{
		index = m_nextFreeComponentIndex++;
	}
	else
	{
		index = m_freeComponentIndices.back();
		m_freeComponentIndices.pop_back();
	}

	Entity *entity = new Entity({ id, m_entityIdVersionMap[id], index });

	// allocate space for future components of this entity
	if (m_entityComponents.size() <= index)
	{
		m_entityComponents.resize(index + 1);
	}

	m_entityComponents[index].m_entity = entity;

	if (notifyListeners)
	{
		for (IOnEntityCreatedListener *listener : m_onEntityCreatedListeners)
		{
			listener->onEntityCreated(entity);
		}
	}

	return entity;
}

void VEngine::EntityManager::destroyEntity(const Entity *entity, bool notifyListeners)
{
	if (notifyListeners)
	{
		for (IOnEntityDestructionListener *listener : m_onEntityDestructionListeners)
		{
			listener->onDestruction(entity);
		}
	}

	Entity::Index index = entity->m_index;

	// delete all components
	for (size_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
	{
		delete m_entityComponents[index].m_components[i];
	}

	// set all pointers to null
	memset(&m_entityComponents[index], 0, sizeof(m_entityComponents[index]));

	Entity::ID id = entity->m_id;
	++m_entityIdVersionMap[id];
	m_freeEntityIds.push_back(id);
	m_freeComponentIndices.push_back(index);

	delete entity;
}

void VEngine::EntityManager::removeComponent(const Entity *entity, IComponent::ComponentTypeID componentTypeId, bool notifyListeners)
{
	Entity::Index index = entity->m_index;
	IComponent *component = m_entityComponents[index].m_components[componentTypeId];

	if (notifyListeners)
	{
		for (IOnComponentRemovedListener *listener : m_onComponentRemovedListeners)
		{
			listener->onComponentRemoved(entity, component);
		}
	}
	
	delete component;
	m_entityComponents[index].m_components[componentTypeId] = nullptr;
}

void VEngine::EntityManager::addOnEntityCreatedListener(IOnEntityCreatedListener *listener)
{
	m_onEntityCreatedListeners.push_back(listener);
}

void VEngine::EntityManager::addOnEntityDestructionListener(IOnEntityDestructionListener *listener)
{
	m_onEntityDestructionListeners.push_back(listener);
}

void VEngine::EntityManager::addOnComponentAddedListener(IOnComponentAddedListener *listener)
{
	m_onComponentAddedListeners.push_back(listener);
}

void VEngine::EntityManager::addOnComponentRemovedListener(IOnComponentRemovedListener *listener)
{
	m_onComponentRemovedListeners.push_back(listener);
}

void VEngine::EntityManager::removeOnEntityCreatedListener(IOnEntityCreatedListener *listener)
{
	ContainerUtility::quickRemove(m_onEntityCreatedListeners, listener);
}

void VEngine::EntityManager::removeOnEntityDestructionListener(IOnEntityDestructionListener *listener)
{
	ContainerUtility::quickRemove(m_onEntityDestructionListeners, listener);
}

void VEngine::EntityManager::removeOnComponentAddedListener(IOnComponentAddedListener *listener)
{
	ContainerUtility::quickRemove(m_onComponentAddedListeners, listener);
}

void VEngine::EntityManager::removeOnComponentRemovedListener(IOnComponentRemovedListener *listener)
{
	ContainerUtility::quickRemove(m_onComponentRemovedListeners, listener);
}
