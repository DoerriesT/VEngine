#pragma once
#include <cstdint>
#include <cassert>

#define SYSTEM_TYPE_COUNT (6)

namespace VEngine
{
	// used as a handle to store systems in containers
	class ISystem
	{
	public:
		using SystemTypeID = uint64_t;

		virtual ~ISystem() = default;
		virtual void init() = 0;
		virtual void input(double time, double timeDelta) = 0;
		virtual void update(double time, double timeDelta) = 0;
		virtual void render() = 0;
		virtual SystemTypeID getTypeIdOfDerived() = 0;

	protected:
		static SystemTypeID m_typeCount;
	};

	// all systems should be derived from this class to be able to determine their type
	template<typename Type>
	class System : public ISystem
	{
	public:
		SystemTypeID getTypeIdOfDerived() override
		{
			return getTypeId();
		}

		static SystemTypeID getTypeId()
		{
			assert(m_typeCount < SYSTEM_TYPE_COUNT);
			static const SystemTypeID type = m_typeCount++;
			return type;
		}
	};
}