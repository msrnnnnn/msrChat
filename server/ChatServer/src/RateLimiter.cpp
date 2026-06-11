/**
 * @file RateLimiter.cpp
 * @brief Per-user 令牌桶限流器实现
 */
#include "RateLimiter.h"
#include <algorithm>

bool RateLimiter::TryAcquire(int uid)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto now = std::chrono::steady_clock::now();
    auto it = _buckets.find(uid);

    if (it == _buckets.end())
    {
        // 新用户，创建桶并消耗一个令牌
        _buckets.emplace(uid, TokenBucket(_per_sec, _burst));
        _buckets[uid].tokens -= 1.0;
        return true;
    }

    auto &bucket = it->second;

    // 补充令牌
    auto elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();
    bucket.tokens = std::min(bucket.max_tokens, bucket.tokens + elapsed * bucket.refill_rate);
    bucket.last_refill = now;

    // 尝试消耗令牌
    if (bucket.tokens >= 1.0)
    {
        bucket.tokens -= 1.0;
        return true;
    }

    return false;
}

void RateLimiter::SetConfig(double per_sec, double burst)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _per_sec = per_sec;
    _burst = burst;
}
