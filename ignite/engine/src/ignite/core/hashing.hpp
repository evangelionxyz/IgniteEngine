// Copyright (C) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_HASHING_HPP
#define IGN_HASHING_HPP

#include "ignite/core/base.hpp"

#include "ignite_rs/core_utils.h"

#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace ignite
{
	struct IGN_API Hashing
	{
		// Hashing functions for various types
		static inline void HashCombine(std::size_t &seed, std::size_t hash)
		{
			seed = static_cast<std::size_t>(ignite_rs_hash_combine(static_cast<uint64_t>(seed), static_cast<uint64_t>(hash)));
		}

		template<typename... Args>
		static inline std::size_t HashCombineAll(const Args&... args)
		{
			std::size_t seed = 0;
			(..., HashCombine(seed, std::hash<std::decay_t<Args>>{}(args)));
			return seed;
		}

		template<typename T>
		static inline std::size_t Hash(const T &value)
		{
			return std::hash<T>{}(value);
		}

		template<typename T>
		static inline std::size_t HashPointer(const T *ptr)
		{
			return std::hash<const T *>{}(ptr);
		}

		template<typename T>
		static inline void HashVector(std::size_t &seed, const std::vector<T> &vec)
		{
			for (const auto &item : vec)
			{
				HashCombine(seed, std::hash<T>{}(item));
			}
		}

		template<typename K, typename V>
		static inline void HashMap(std::size_t &seed, const std::unordered_map<K, V> &map)
		{
			for (const auto &[key, value] : map)
			{
				HashCombine(seed, std::hash<K>{}(key));
				HashCombine(seed, std::hash<V>{}(value));
			}
		}
	};
}

#endif
