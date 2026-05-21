/**
 * @file ProtocolStructs.h
 * @brief 网络协议强类型数据结构体
 * @details 定义所有网络回包的强类型结构，消除UI层对JSON的直接依赖
 */
#ifndef PROTOCOLSTRUCTS_H
#define PROTOCOLSTRUCTS_H

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct LoginReqStruct
{
    QString user;
    QString passwd;
};

struct ChatLoginReqStruct
{
    int uid;
    QString token;
};

struct ChatTextReqStruct
{
    int from_uid;
    int to_uid;
    QString content;
    QString client_msg_id;
};

struct LoginRspStruct
{
    int error;
    int uid;
    QString token;
    QString user;
};

struct ChatLoginRspStruct
{
    int error;
    QString message;
};

struct ChatTextMsgStruct
{
    int from_uid;
    int to_uid;
    QString content;
    QString client_msg_id;
    qint64 server_msg_id;
    qint64 timestamp;
};

struct ChatAckStruct
{
    int error;
    QString message;
    QString client_msg_id;
};

struct OfflineAckStruct
{
    qint64 received;
    qint64 total;
};

struct OfflineAckReqStruct
{
    qint64 received;
};

struct VerifyCodeReqStruct
{
    QString email;
};

struct VerifyCodeRspStruct
{
    int error;
    QString email;
    int code = 0;
};

struct RegisterReqStruct
{
    QString user;
    QString email;
    QString passwd;
    QString varifycode;
};

struct RegisterRspStruct
{
    int error;
    QString email;
};

struct ResetPwdReqStruct
{
    QString user;
    QString email;
    QString passwd;
    QString varifycode;
};

struct ResetPwdRspStruct
{
    int error;
};

Q_DECLARE_METATYPE(ChatTextMsgStruct)
Q_DECLARE_METATYPE(ChatAckStruct)
Q_DECLARE_METATYPE(OfflineAckStruct)
Q_DECLARE_METATYPE(LoginRspStruct)
Q_DECLARE_METATYPE(ChatLoginRspStruct)

struct FileReqStruct
{
    int64_t task_id;
    int from_uid;
    int to_uid;
    QString filename;
    int64_t total_size;
    QString md5;
    int64_t offset = 0;
};

#endif // PROTOCOLSTRUCTS_H
