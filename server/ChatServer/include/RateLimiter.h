#pragma once
/**
 * @file RateLimiter.h
 * @brief Per-user 令牌桶限流器
 * @details 基于令牌桶算法，为每个用户维护独立的限流状态。
 *          用于消息发送频率限制，防止洪泛攻击。
 */
#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <chrono>
#include <mutex>
#include <unordered_map>

/**
 * @brief 令牌桶
 */
struct TokenBucket
{
    double tokens;                          ///< 当前令牌数
    double max_tokens;                      ///< 桶容量（突发上限）
    double refill_rate;                     ///< 每秒补充令牌数
    std::chrono::steady_clock::time_point last_refill;  ///< 上次补充时间

    TokenBucket() : tokens(0), max_tokens(0), refill_rate(0),
        last_refill(std::chrono::steady_clock::now()) {}

    TokenBucket(double rate, double burst)
        : tokens(burst), max_tokens(burst), refill_rate(rate),
          last_refill(std::chrono::steady_clock::now())
    {
    }
};

/**
 * @brief Per-user 令牌桶限流器（单例）
 * @details 每个用户独立一个令牌桶，互不影响。
 *          配置项从 config.ini 读取：
 *          - RateLimitPerSec：每秒允许的消息数（默认 10）
 *          - RateLimitBurst：突发上限（默认 20）
 */
class RateLimiter
{
public:
    static RateLimiter &Instance()
    {
        static RateLimiter instance;
        return instance;
    }

    /**
     * @brief 尝试获取一个令牌
     * @param uid 用户 ID
     * @return true 表示允许，false 表示被限流
     */
    bool TryAcquire(int uid);

    /**
     * @brief 设置限流参数
     * @param per_sec 每秒允许的消息数
     * @param burst 突发上限
     */
    void SetConfig(double per_sec, double burst);

private:
    RateLimiter() = default;

    mutable std::mutex _mutex;
    std::unordered_map<int, TokenBucket> _buckets;
    double _per_sec{10.0};
    double _burst{20.0};
};

#endif // RATE_LIMITER_H
