#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <bitset>
#include "Component/Component.h"
#include "Utility/ContainerUtility.h"
#include "IOnComponentAddedListener.h"
#include "IOnComponentRemovedListener.h"
#include "IOnEntityCreatedListener.h"
#include "IOnEntityDestructionListener.h"

namespace VEngine
{
	class EntityManager;

	struct Entity
	{
		friend class EntityManager;
	private:
		using Id = uint64_t;
		using Version = uint64_t;

		Entity(const Id &_id, const Id &_version);
		~Entity() = default;

		const Id m_id;
		const Version m_version;
	};

	class EntityManager
	{
	public:
		EntityManager() = default;
		EntityManager(const EntityManager &) = delete;
		EntityManager(const EntityManager &&) = delete;
		EntityManager &operator= (const EntityManager &) = delete;
		EntityManager &operator= (const EntityManager &&) = delete;
		~EntityManager() = default;
		const Entity *createEntity();
		template<typename ComponentType, typename ...Args>
		ComponentType *addComponent(const Entity *entity, Args &&...args);
		template<typename ComponentType>
		void removeComponent(const Entity *entity, bool notify = true);
		void removeComponent(const Entity *entity, uint64_t componentTypeId, bool notify = true);
		template<typename ComponentType>
		ComponentType *getComponent(const Entity *entity);
		void destroyEntity(const Entity *entity);
		std::bitset<COMPONENT_TYPE_COUNT> getComponentBitSet(const Entity *entity);
		std::unordered_map<uint64_t, BaseComponent *> getComponentMap(const Entity *entity);
		void addOnEntityCreatedListener(IOnEntityCreatedListener *listener);
		void addOnEntityDestructionListener(IOnEntityDestructionListener *listener);
		void addOnComponentAddedListener(IOnComponentAddedListener *listener);
		void addOnComponentRemovedListener(IOnComponentRemovedListener *listener);
		void removeOnEntityCreatedListener(IOnEntityCreatedListener *listener);
		void removeOnEntityDestructionListener(IOnEntityDestructionListener *listener);
		void removeOnComponentAddedListener(IOnComponentAddedListener *listener);
		void removeOnComponentRemovedListener(IOnComponentRemovedListener *listener);

	private:
		std::uint64_t m_nextFreeId;
		std::vector<uint64_t> m_freeIds;
		std::unordered_map<Entity::Id, std::bitset<COMPONENT_TYPE_COUNT>> m_entityIdToComponentBitSetMap;
		std::unordered_map<Entity::Id, std::unordered_map<uint64_t, BaseComponent *>> m_entityIdToComponentMap;
		std::unordered_map<Entity::Id, Entity::Version> m_entityIdVersionMap;
		std::vector<IOnEntityCreatedListener *> m_onEntityCreatedListeners;
		std::vector<IOnEntityDestructionListener *> m_onEntityDestructionListeners;
		std::vector<IOnComponentAddedListener *> m_onComponentAddedListeners;
		std::vector<IOnComponentRemovedListener *> m_onComponentRemovedListeners;

		bool validateEntity(const Entity *entity);
	};

	template<typename ComponentType, typename ...Args>
	ComponentType *EntityManager::addComponent(const Entity *entity, Args&& ...args)
	{
		assert(validateEntity(entity));

		const Entity::Id &id = entity->m_id;

		uint64_t typeId = ComponentType::getTypeId();

		// if a component of this type already exists, remove it
		if (m_entityIdToComponentBitSetMap[id] & typeId)
		{
			removeComponent<ComponentType>(entity);
		}

		// set the type and family bit
		m_entityIdToComponentBitSetMap[id].set(typeId);
		m_entityIdToFamilyBitFieldMap[id].set(familyId);

		// create the new component
		ComponentType *component = new ComponentType(std::forward<Args>(_args)...);

		// add the component to type and family maps
		m_entityIdToComponentMap[id].emplace(typeId, component);
		m_entityIdToFamilyMap[id].emplace(familyId, component);

		//invoke listeners
		for (IOnComponentAddedListener *listener : m_onComponentAddedListeners)
		{
			listener->onComponentAdded(_entity, component);
		}

		return component;
	}

	template<typename ComponentType>
	void EntityManager::removeComponent(const Entity *entity, bool notify)
	{
		assert(validateEntity(entity));

		uint64_t typeId = ComponentType::getTypeId();
		removeComponent(entity, typeId, notify);
	}

	template<typename ComponentType>
	inline ComponentType *EntityManager::getComponent(const Entity *entity)
	{
		assert(entity);

		ComponentType *component = nullptr;

		//assert that there is a bitmap attached to the entity
		assert(ContainerUtility::contains(m_entityIdToComponentBitSetMap, entity->m_id));

		// check if the component is attached to the entity
		if ((m_entityIdToComponentBitSetMap[entity->m_id] & ComponentType::getTypeId()) == ComponentType::getTypeId())
		{
			assert(ContainerUtility::contains(m_entityIdToComponentMap[entity->m_id], ComponentType::getTypeId()));
			component = static_cast<ComponentType *>(m_entityIdToComponentMap[entity->m_id][ComponentType::getTypeId()]);
		}

		return component;
	}

}