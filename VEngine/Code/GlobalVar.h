#pragma once
#include <vector>
#include <functional>

namespace VEngine
{
	template<typename Type>
	class GlobalVar
	{
	public:
		GlobalVar(const Type &value);
		Type get() const;
		void set(const Type &value);
		void addListener(std::function<void(const Type &)> listener);
		operator Type() const;

	private:
		Type m_value;
		std::vector<std::function<void(const Type &)>> m_listeners;
	};

	template<typename Type>
	inline GlobalVar<Type>::GlobalVar(const Type &value)
		:m_value(value)
	{
	}

	template<typename Type>
	inline Type GlobalVar<Type>::get() const
	{
		return m_value;
	}

	template<typename Type>
	inline void GlobalVar<Type>::set(const Type &value)
	{
		if (value != m_value)
		{
			m_value = value;
			for (auto &listener : m_listeners)
			{
				listener(m_value);
			}
		}
	}

	template<typename Type>
	inline void GlobalVar<Type>::addListener(std::function<void(const Type&)> listener)
	{
		m_listeners.push_back(listener);
	}

	template<typename Type>
	inline GlobalVar<Type>::operator Type() const
	{
		return m_value;
	}
}