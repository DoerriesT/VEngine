#pragma once
#include <cstdint>
#include <vector>
#include <bitset>
#include "ECS/Component/Component.h"
#include "ECS/IOnComponentAddedListener.h"
#include "ECS/IOnComponentRemovedListener.h"
#include "ECS/IOnEntityDestructionListener.h"

namespace VEngine
{
	class BaseSystem
		: public IOnComponentAddedListener,
		public IOnComponentRemovedListener,
		public IOnEntityDestructionListener
	{
	public:
		virtual ~BaseSystem() = default;
		virtual void init() = 0;
		virtual void input(double currentTime, double timeDelta) = 0;
		virtual void update(double currentTime, double timeDelta) = 0;
		virtual void render() = 0;
		virtual uint64_t getTypeIdOfDerived() = 0;
		void onComponentAdded(const Entity *entity, BaseComponent *addedComponent) override = 0;
		void onComponentRemoved(const Entity *entity, BaseComponent *removedComponent) override = 0;
		void onDestruction(const Entity *entity) override = 0;

	protected:
		static uint64_t m_typeCount;
		std::vector<const Entity *> m_managedEntities;
		std::vector<std::bitset<COMPONENT_TYPE_COUNT>> m_validBitSets;

		bool validate(const std::bitset<COMPONENT_TYPE_COUNT> &componentBitSet);
	};

	template<typename Type>
	class System : public BaseSystem
	{
	public:
		typedef Type SystemType;

		static uint64_t getTypeId();
		uint64_t getTypeIdOfDerived() override { return getTypeId(); }
	};

	template<typename Type>
	inline std::uint64_t System<Type>::getTypeId()
	{
		static const uint64_t type = m_typeCount++;
		return type;
	}
}