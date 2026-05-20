#ifndef TOKEN_MANAGER_H
#define TOKEN_MANAGER_H

#include "ShardedMap.h"
#include <memory>
#include <string>

class CSession;

class TokenManager
{
public:
    static TokenManager &Instance()
    {
        static TokenManager instance;
        return instance;
    }

    void SetToken(int uid, const std::string &token);
    bool CheckToken(int uid, const std::string &token);
    void RemoveToken(int uid);
    std::string GetToken(int uid) const;
    void LoadTokensFromDB();

private:
    TokenManager() = default;
    ~TokenManager() = default;

    TokenManager(const TokenManager &) = delete;
    TokenManager &operator=(const TokenManager &) = delete;

    TokenManager(TokenManager &&) = delete;
    TokenManager &operator=(TokenManager &&) = delete;

    ShardedMap<int, std::string> _uid_tokens{16};
};

#endif
