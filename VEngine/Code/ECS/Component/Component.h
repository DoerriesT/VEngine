#pragma once
#include <cstdint>
#include <cassert>

#define COMPONENT_TYPE_COUNT (64)

namespace VEngine
{
	// used as a handle to store components in containers
	struct IComponent
	{
	public:
		using ComponentTypeID = uint64_t;

		virtual ~IComponent() = default;
		virtual ComponentTypeID getTypeIdOfDerived() = 0;

	protected:
		static ComponentTypeID m_typeCount;
	};

	// all components should be derived from this class to be able to determine their type
	template<typename Type>
	struct Component : public IComponent
	{
		ComponentTypeID getTypeIdOfDerived() override
		{
			return getTypeId();
		}

		static ComponentTypeID getTypeId()
		{
			assert(m_typeCount < COMPONENT_TYPE_COUNT);
			static const ComponentTypeID type = m_typeCount++;
			return type;
		}
	};
}