#pragma once

namespace VEngine
{
	template<typename T, size_t Count>
	class StaticObjectPool
	{
	public:
		explicit StaticObjectPool();
		T *alloc();
		void free(T *value);
		size_t getAllocationCount() const;

	private:
		union Item
		{
			size_t m_nextFreeItem;
			T m_value;
		};

		Item m_items[Count];
		size_t m_firstFreeIndex;
		size_t m_allocationCount;
	};

	template<typename T, size_t Count>
	inline StaticObjectPool<T, Count>::StaticObjectPool()
	{
		// setup linked list
		for (size_t i = 0; i < Count; ++i)
		{
			m_items[i].m_nextFreeItem = i + 1;
		}

		m_items[Count - 1].m_nextFreeItem = ~size_t(0);
	}

	template<typename T, size_t Count>
	inline T *StaticObjectPool<T, Count>::alloc()
	{
		if (m_firstFreeIndex == ~size_t(0))
		{
			return nullptr;
		}

		++m_allocationCount;

		Item &item = m_items[m_firstFreeIndex];
		m_firstFreeIndex = item.m_nextFreeItem;
		return &item.m_value;
	}

	template<typename T, size_t Count>
	inline void StaticObjectPool<T, Count>::free(T *value)
	{
		Item *item = (Item *)value;
		size_t index = item - m_items;
		item->m_nextFreeItem = m_firstFreeIndex;
		m_firstFreeIndex = index;

		--m_allocationCount;
	}

	template<typename T, size_t Count>
	inline size_t StaticObjectPool<T, Count>::getAllocationCount() const
	{
		return m_allocationCount;
	}
}