#pragma once
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <algorithm>

namespace VEngine
{
	namespace ContainerUtility
	{
		template<typename T>
		inline bool find(const std::vector<T> &v, const T &item, int &position)
		{
			auto position = std::find(v.cbegin(), v.cend(), item);

			if (position < v.cend())
			{
				position = (int)(position - v.cbegin());
				return true;
			}
			return false;
		}

		template<typename T>
		inline bool contains(const std::vector<T> &v, const T &item)
		{
			return std::find(v.cbegin(), v.cend(), item) != v.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::map<Key, Value> &m, const Key &item)
		{
			return m.find(item) != m.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::unordered_map<Key, Value> &m, const Key &item)
		{
			return m.find(item) != m.cend();
		}

		template<typename T>
		inline bool contains(const std::set<T> &s, const T &item)
		{
			return s.find(item) != s.cend();
		}

		template<typename T>
		inline bool contains(const std::unordered_set<T> &set, const T &item)
		{
			return set.find(item) != set.cend();
		}

		template<typename T>
		inline void remove(std::vector<T> &v, const T &item)
		{
			v.erase(std::remove(v.begin(), v.end(), item), v.end());
		}

		template<typename Key, typename Value>
		inline void remove(std::map<Key, Value> &m, const Key &item)
		{
			m.erase(item);
		}

		template<typename Key, typename Value>
		inline void remove(std::unordered_map<Key, Value> &m, const Key &item)
		{
			m.erase(item);
		}

		template<typename T>
		inline void remove(std::set<T> &s, const T &item)
		{
			s.erase(item);
		}

		template<typename T>
		inline void remove(std::unordered_set<T> &s, const T &item)
		{
			s.erase(item);
		}

		template<typename T>
		inline void quickRemove(std::vector<T> &v, const T &item)
		{
			auto it = std::find(v.begin(), v.end(), item);
			if (it != v.end())
			{
				std::swap(v.back(), *it);
				v.erase(--v.end());
			}
		}
	}
}