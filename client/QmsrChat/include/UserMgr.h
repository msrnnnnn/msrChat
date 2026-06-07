/**
 * @file usermgr.h
 * @brief 用户数据管理单例类
 * @details 存储当前登录用户的 UID、用户名和 Token。
 */
#ifndef USERMGR_H
#define USERMGR_H

#include "Singleton.h"
#include <QObject>
#include <QString>

class UserMgr : public Singleton<UserMgr>
{
    friend class Singleton<UserMgr>;

public:
    static UserMgr* Instance() {
        return Singleton<UserMgr>::Instance();
    }

    static void Init() {
        Singleton<UserMgr>::Init();
    }

    static void Destroy() {
        Singleton<UserMgr>::Destroy();
    }

    void SetUid(int uid);
    int GetUid() const;

    void SetToken(const QString &token);
    QString GetToken() const;

private:
    UserMgr() = default;

    int _uid = 0;
    QString _token = "";
};

#endif
