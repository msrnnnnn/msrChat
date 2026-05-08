#ifndef SHARDED_MAP_H
#define SHARDED_MAP_H

#include <functional>
#include <mutex>
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

    Value *Find(const Key &key)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    const Value *Find(const Key &key) const
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    void Erase(const Key &key)
    {
        auto &shard = _shards[GetShardIndex(key)];
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.data.erase(key);
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

    std::unique_lock<std::mutex> GetLock(std::size_t idx) const
    {
        return std::unique_lock<std::mutex>(_shards[idx].mutex);
    }

    const std::unordered_map<Key, Value> &GetShard(std::size_t idx) const
    {
        return _shards[idx].data;
    }
};

#endif
