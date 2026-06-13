#pragma once
/**
 * @file NonceCache.h
 * @brief 消息防重放 Nonce 缓存（LRU + TTL）
 * @details Phase 5E.1 — 防止消息重放攻击。
 *          每个 nonce 在 TTL（默认 5 分钟）内唯一，超过容量时按 LRU 淘汰。
 */
#ifndef NONCECACHE_H
#define NONCECACHE_H

#include <chrono>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

class NonceCache
{
public:
    static NonceCache &Instance()
    {
        static NonceCache instance;
        return instance;
    }

    /**
     * @brief 检查 nonce 是否已存在，不存在则插入
     * @param nonce 客户端生成的唯一字符串
     * @return true = 新 nonce（合法），false = 重复（重放攻击）
     */
    bool TryInsert(const std::string &nonce)
    {
        if (nonce.empty()) return true;  // 空 nonce 视为可选字段未设置，跳过

        std::lock_guard<std::mutex> lock(_mutex);
        EvictExpired();

        auto it = _cache.find(nonce);
        if (it != _cache.end())
        {
            return false;  // 重复
        }

        // 容量检查
        if (_cache.size() >= _max_size)
        {
            // 淘汰最旧的
            if (!_lru_order.empty())
            {
                std::string oldest = _lru_order.back();
                _lru_order.pop_back();
                _cache.erase(oldest);
            }
        }

        auto now = std::chrono::steady_clock::now();
        _cache[nonce] = now;
        _lru_order.push_front(nonce);
        return true;
    }

    /**
     * @brief 校验消息时间戳是否在允许窗口内
     * @param msg_timestamp_ms 消息中的毫秒时间戳
     * @return true = 在窗口内，false = 超时
     */
    bool IsWithinTimeWindow(int64_t msg_timestamp_ms) const
    {
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t diff_sec = std::abs(now_ms - msg_timestamp_ms) / 1000;
        return diff_sec <= _ttl_seconds;
    }

private:
    NonceCache() = default;
    NonceCache(const NonceCache &) = delete;
    NonceCache &operator=(const NonceCache &) = delete;

    void EvictExpired()
    {
        auto now = std::chrono::steady_clock::now();
        auto ttl = std::chrono::seconds(_ttl_seconds);

        while (!_lru_order.empty())
        {
            const std::string &oldest = _lru_order.back();
            auto it = _cache.find(oldest);
            if (it == _cache.end() || (now - it->second) > ttl)
            {
                if (it != _cache.end()) _cache.erase(it);
                _lru_order.pop_back();
            }
            else
            {
                break;  // 剩余的都还在有效期内
            }
        }
    }

    mutable std::mutex _mutex;
    size_t _max_size = 10000;
    int _ttl_seconds = 300;  // 5 分钟
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _cache;
    std::list<std::string> _lru_order;  // 前端 = 最近使用
};

#endif // NONCECACHE_H
