#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class ShardedMap
{
public:
    explicit ShardedMap(std::size_t shard_count = 32)
        : _shard_count(std::max<std::size_t>(1, shard_count)),
          _maps(_shard_count),
          _mutexes(_shard_count)
    {
    }

    void Insert(const K &key, V value)
    {
        auto shard_index = GetShardIndex(key);
        std::unique_lock<std::shared_mutex> lock(_mutexes[shard_index]);
        _maps[shard_index][key] = std::move(value);
    }

    void Insert(K &&key, V value)
    {
        auto shard_index = GetShardIndex(key);
        std::unique_lock<std::shared_mutex> lock(_mutexes[shard_index]);
        _maps[shard_index][std::move(key)] = std::move(value);
    }

    bool Erase(const K &key)
    {
        auto shard_index = GetShardIndex(key);
        std::unique_lock<std::shared_mutex> lock(_mutexes[shard_index]);
        return _maps[shard_index].erase(key) > 0;
    }

    std::optional<V> Find(const K &key) const
    {
        auto shard_index = GetShardIndex(key);
        std::shared_lock<std::shared_mutex> lock(_mutexes[shard_index]);
        const auto &shard = _maps[shard_index];
        auto it = shard.find(key);
        if (it == shard.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    bool Contains(const K &key) const
    {
        return Find(key).has_value();
    }

    std::size_t ShardCount() const
    {
        return _shard_count;
    }

private:
    std::size_t GetShardIndex(const K &key) const
    {
        return _hash(key) % _shard_count;
    }

    std::size_t _shard_count;
    Hash _hash;
    std::vector<std::unordered_map<K, V, Hash, KeyEqual>> _maps;
    mutable std::vector<std::shared_mutex> _mutexes;
};
