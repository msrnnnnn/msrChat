#include "TokenManager.h"

void TokenManager::SetToken(int uid, const std::string &token)
{
    _uid_tokens.Insert(uid, token);
}

bool TokenManager::CheckToken(int uid, const std::string &token)
{
    auto stored_token = _uid_tokens.Find(uid);
    if (!stored_token)
    {
        return false;
    }
    return *stored_token == token;
}

void TokenManager::RemoveToken(int uid)
{
    _uid_tokens.Erase(uid);
}

std::string TokenManager::GetToken(int uid) const
{
    auto token = _uid_tokens.Find(uid);
    return token ? *token : std::string();
}
