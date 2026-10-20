#pragma once


namespace jh
{
	template <typename T>
	using Vector = std::vector<T, MySTLAllocator<T>>;

	template <typename T>
	using Deque = std::deque<T, MySTLAllocator<T>>;

	template <typename T>
	using Queue = std::queue<T, Deque<T>>;

	template <typename Key, typename Value>
	using Unordered_map = std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, MySTLAllocator<std::pair<const Key, Value>>>;

	template <typename Key, typename Value>
	using Map = std::map<Key, Value, std::less<Key>, MySTLAllocator<std::pair<const Key, Value>>>;
}