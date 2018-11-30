#pragma once
#include <vector>
#include <unordered_map>
#include <functional>
#include <cassert>
#include "Component/Component.h"
#include "IOnComponentAddedListener.h"
#include "IOnComponentRemovedListener.h"
#include "IOnEntityCreatedListener.h"
#include "IOnEntityDestructionListener.h"

namespace VEngine
{
	// Components are stored in an array together with each Entity. The array can be indexed with the Component type.
	// Each Entity stores an index into a vector with Entity/Component data. Each row in the diagram
	// represents one element in the Entity/Component vector.
	//
	//     C0  C1  C2
	// E0  X   X
	// E1      X   X
	// E2  X       X
	//
	// When an entity is destroyed, its index is added to a list of indices to be recycled

	struct Entity
	{
		using ID = uint64_t;
		using Version = uint64_t;
		using Index = size_t;
		ID m_id;
		Version m_version;
		Index m_index;
	};

	class EntityManager
	{
	public:
		template<typename T>
		struct Identity
		{
			using type = T;
		};

		explicit EntityManager();
		~EntityManager();
		const Entity *createEntity(bool notifyListeners = true);
		void destroyEntity(const Entity *entity, bool notifyListeners = true);
		template<typename ComponentType, typename ...Args>
		ComponentType *addComponent(const Entity *entity, Args&& ...args);
		void removeComponent(const Entity *entity, IComponent::ComponentTypeID componentTypeId, bool notifyListeners = true);
		template<typename ComponentType>
		ComponentType *getComponent(const Entity *entity);
		template<typename ...Components>
		void each(const typename Identity<std::function<void(const Entity *, Components&...)>>::type &func);
		void addOnEntityCreatedListener(IOnEntityCreatedListener *listener);
		void addOnEntityDestructionListener(IOnEntityDestructionListener *listener);
		void addOnComponentAddedListener(IOnComponentAddedListener *listener);
		void addOnComponentRemovedListener(IOnComponentRemovedListener *listener);
		void removeOnEntityCreatedListener(IOnEntityCreatedListener *listener);
		void removeOnEntityDestructionListener(IOnEntityDestructionListener *listener);
		void removeOnComponentAddedListener(IOnComponentAddedListener *listener);
		void removeOnComponentRemovedListener(IOnComponentRemovedListener *listener);

	private:
		struct EntityComponents
		{
			const Entity *m_entity;
			IComponent *m_components[COMPONENT_TYPE_COUNT];
		};

		Entity::ID m_nextFreeEntityId;
		std::vector<Entity::ID> m_freeEntityIds;
		std::unordered_map<Entity::ID, Entity::Version> m_entityIdVersionMap;
		std::vector<EntityComponents> m_entityComponents;
		Entity::Index m_nextFreeComponentIndex;
		std::vector<Entity::Index> m_freeComponentIndices;
		std::vector<IOnEntityCreatedListener *> m_onEntityCreatedListeners;
		std::vector<IOnEntityDestructionListener *> m_onEntityDestructionListeners;
		std::vector<IOnComponentAddedListener *> m_onComponentAddedListeners;
		std::vector<IOnComponentRemovedListener *> m_onComponentRemovedListeners;
	};

	template<typename ComponentType, typename ...Args>
	inline ComponentType *EntityManager::addComponent(const Entity *entity, Args &&...args)
	{
		assert(m_entityIdVersionMap.find(entity->m_id) != m_entityIdVersionMap.end() && entity->m_version == m_entityIdVersionMap[entity->m_id]);

		Entity::Index index = entity->m_index;
		const IComponent::ComponentTypeID componentId = ComponentType::getTypeId();

		assert(index < m_entityComponents.size());
		assert(componentId < COMPONENT_TYPE_COUNT);

		// if a component of this type already exists, remove it
		if (m_entityComponents[index].m_components[componentId])
		{
			removeComponent(entity, componentId);
		}

		// create new component
		ComponentType *component = new ComponentType(std::forward<Args>(args)...);
		m_entityComponents[index].m_components[componentId] = component;

		for (IOnComponentAddedListener *listener : m_onComponentAddedListeners)
		{
			listener->onComponentAdded(entity, component);
		}

		return component;
	}

	template<typename ComponentType>
	inline ComponentType *EntityManager::getComponent(const Entity *entity)
	{
		assert(m_entityIdVersionMap.find(entity->m_id) != m_entityIdVersionMap.end() && entity->m_version == m_entityIdVersionMap[entity->m_id]);

		return static_cast<ComponentType *>(m_entityComponents[entity->m_index].m_components[ComponentType::getTypeId()]);
	}

	template<typename ...Components>
	inline void EntityManager::each(const typename Identity<std::function<void(const Entity *, Components&...)>>::type &func)
	{
		for (auto &entityComponents : m_entityComponents)
		{
			// skip empty entries
			if (!entityComponents.m_entity)
			{
				continue;
			}

			// test if all required components are present
			bool presentComponents[sizeof...(Components)] = { (static_cast<bool>(entityComponents.m_components[Components::getTypeId()]))... };
			bool allPresent = true;

			for (size_t j = 0; j < sizeof(presentComponents) / sizeof(presentComponents[0]); ++j)
			{
				if (!presentComponents[j])
				{
					allPresent = false;
					break;
				}
			}

			if (allPresent)
			{
				func(entityComponents.m_entity, *static_cast<Components *>(entityComponents.m_components[Components::getTypeId()])...);
			}
		}
	}
}