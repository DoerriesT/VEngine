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
		inline bool find(const std::vector<T> &_vector, const T &_item, int &_position)
		{
			auto position = std::find(_vector.cbegin(), _vector.cend(), _item);

			if (position < _vector.cend())
			{
				_position = (int)(position - _vector.cbegin());
				return true;
			}
			return false;
		}

		template<typename T>
		inline bool contains(const std::vector<T> &_vector, const T &_item)
		{
			return std::find(_vector.cbegin(), _vector.cend(), _item) != _vector.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::map<Key, Value> &_map, const Key &_item)
		{
			return _map.find(_item) != _map.cend();
		}

		template<typename Key, typename Value>
		inline bool contains(const std::unordered_map<Key, Value> &_map, const Key &_item)
		{
			return _map.find(_item) != _map.cend();
		}

		template<typename T>
		inline bool contains(const std::set<T> &_set, const T &_item)
		{
			return _set.find(_item) != _set.cend();
		}

		template<typename T>
		inline bool contains(const std::unordered_set<T> &_set, const T &_item)
		{
			return _set.find(_item) != _set.cend();
		}

		template<typename T>
		inline void remove(std::vector<T> &_vector, const T &_item)
		{
			_vector.erase(std::remove(_vector.begin(), _vector.end(), _item), _vector.end());
		}

		template<typename Key, typename Value>
		inline void remove(std::map<Key, Value> &_map, const Key &_item)
		{
			_map.erase(_item);
		}

		template<typename Key, typename Value>
		inline void remove(std::unordered_map<Key, Value> &_map, const Key &_item)
		{
			_map.erase(_item);
		}

		template<typename T>
		inline void remove(std::set<T> &_set, const T &_item)
		{
			_set.erase(_item);
		}

		template<typename T>
		inline void remove(std::unordered_set<T> &_set, const T &_item)
		{
			_set.erase(_item);
		}
	}
}