#pragma once
#include <cstdint>

#define COMPONENT_TYPE_COUNT (3)

namespace VEngine
{
	enum class Mobility
	{
		STATIC, DYNAMIC
	};

	// used as a handle to store components in containers
	class BaseComponent
	{
	public:
		virtual ~BaseComponent() = default;
		virtual std::uint64_t getTypeIdOfDerived() = 0;

	protected:
		static std::uint64_t m_typeCount;
	};

	// all components should be derived from this class to be able to determine their type
	template<typename Type>
	class Component : public BaseComponent
	{
	public:
		typedef Type ComponentType;

		static std::uint64_t getTypeId();
		std::uint64_t getTypeIdOfDerived() override { return getTypeId(); }
	};

	template<typename Type>
	inline std::uint64_t Component<Type>::getTypeId()
	{
		static const std::uint64_t type = m_typeCount++;
		return type;
	}
}