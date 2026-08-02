#pragma once
/**
 * @file ShardedMap.h
 * @brief 分片哈希表 —— 将数据按 key 哈希分散到多个分片，每个分片独立加锁
 * @details 通过增加分片数来降低锁竞争，提高多线程并发读写性能。
 *          每个分片是一个 `std::unordered_map` + `std::mutex` 的组合。
 */
#ifndef SHARDED_MAP_H
#define SHARDED_MAP_H

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

/**
 * @brief 线程安全的分片哈希表
 * @tparam Key 键类型
 * @tparam Value 值类型
 * @details 操作时仅锁定 key 所在的分片，不同分片之间可并发访问。
 *          ForEach/Size 等全局操作会依次锁定所有分片（非同时），但不保证快照一致性。
 */
template <typename Key, typename Value> class ShardedMap
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
    explicit ShardedMap(std::size_t shard_count) : _shards(shard_count)
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

    /**
     * @brief 条件删除 —— 仅当 key 存在且 predicate 返回 true 时才删除
     * @param key 键
     * @param pred 谓词函数，接收 Value 引用，返回 true 表示删除
     * @return true 找到并删除成功；false key 不存在或谓词返回 false
     */
    template <typename Pred> bool RemoveIfMatch(const Key &key, Pred &&pred)
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

    /**
     * @brief 遍历所有分片中的所有键值对（逐个分片加锁，不保证跨分片快照一致性）
     */
    template <typename Func> void ForEach(Func &&func)
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

    /**
     * @brief 遍历所有分片中的所有键值对（const 版本，逐个分片加锁）
     */
    template <typename Func> void ForEach(Func &&func) const
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
