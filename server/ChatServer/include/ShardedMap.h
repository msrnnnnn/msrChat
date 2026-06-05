#ifndef SHARDED_MAP_H
#define SHARDED_MAP_H

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

template<typename Key, typename Value>
class ShardedMap
{
    struct Shard
    {
        std::unordered_map<Key, Value> data;
        mutable std::mutex mutex;
    };

    std::vector<Shard> _shards;

    std::size_t GetShardIndex(const Key &key) const
    {
        return std::hash<Key>{}(key) % _shards.size();
    }

public:
    explicit ShardedMap(std::size_t shard_count)
        : _shards(shard_count)
    {
    }

    void Insert(const Key &key, const Value &value)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.data[key] = value;
    }

    std::optional<Value> Find(const Key &key)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<Value> Find(const Key &key) const
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void Erase(const Key &key)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.data.erase(key);
    }

    template <typename Pred>
    bool RemoveIfMatch(const Key &key, Pred &&pred)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end())
        {
            return false;
        }
        if (!std::forward<Pred>(pred)(it->second))
        {
            return false;
        }
        shard.data.erase(it);
        return true;
    }

    std::size_t ShardCount() const
    {
        return _shards.size();
    }

    void Clear()
    {
        for (auto &shard : _shards)
        {
            std::lock_guard<std::mutex> lock(shard.mutex);
            shard.data.clear();
        }
    }

    template <typename Func>
    void ForEach(Func &&func)
    {
        for (std::size_t i = 0; i < _shards.size(); ++i)
        {
            std::lock_guard<std::mutex> lock(_shards[i].mutex);
            for (const auto &[key, value] : _shards[i].data)
            {
                func(key, value);
            }
        }
    }

    template <typename Func>
    void ForEach(Func &&func) const
    {
        for (std::size_t i = 0; i < _shards.size(); ++i)
        {
            std::lock_guard<std::mutex> lock(_shards[i].mutex);
            for (const auto &[key, value] : _shards[i].data)
            {
                func(key, value);
            }
        }
    }

    std::size_t Size() const
    {
        std::size_t total = 0;
        for (std::size_t i = 0; i < _shards.size(); ++i)
        {
            std::lock_guard<std::mutex> lock(_shards[i].mutex);
            total += _shards[i].data.size();
        }
        return total;
    }
};

#endif
