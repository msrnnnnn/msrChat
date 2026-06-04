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

// === 图片消息 (Phase 3 新增) ===
struct ChatImageStruct
{
    int from_uid = 0;
    int to_uid = 0;
    QString image_id;
    QString caption;
    qint64 timestamp = 0;
    int width = 0;
    int height = 0;
    QString ext;
    qint64 size = 0;
    QString md5;
};

struct ImageDownloadRspStruct
{
    int error = 0;
    QString image_id;
    qint64 offset = 0;
};

// === 撤回 (Phase 3 新增) ===
struct ChatRecallMsgStruct
{
    int from_uid = 0;
    qint64 msg_timestamp = 0;
    QString client_msg_id;
};

struct ChatRecallNotifyStruct
{
    qint64 msg_timestamp = 0;
    int recall_uid = 0;
    int recalled_to = 0;
    qint64 recall_ts = 0;
};

// === 编辑 (Phase 3 新增) ===
struct ChatEditMsgStruct
{
    int from_uid = 0;
    qint64 msg_timestamp = 0;
    QString new_content;
};

struct ChatEditAckStruct
{
    int error = 0;
    QString message;
    qint64 msg_timestamp = 0;
    qint64 edit_ts = 0;
};

struct ChatEditNotifyStruct
{
    qint64 msg_timestamp = 0;
    int from_uid = 0;
    QString new_content;
    qint64 edit_ts = 0;
};

Q_DECLARE_METATYPE(ChatImageStruct)
Q_DECLARE_METATYPE(ImageDownloadRspStruct)
Q_DECLARE_METATYPE(ChatRecallMsgStruct)
Q_DECLARE_METATYPE(ChatRecallNotifyStruct)
Q_DECLARE_METATYPE(ChatEditMsgStruct)
Q_DECLARE_METATYPE(ChatEditAckStruct)
Q_DECLARE_METATYPE(ChatEditNotifyStruct)

#endif // PROTOCOLSTRUCTS_H
