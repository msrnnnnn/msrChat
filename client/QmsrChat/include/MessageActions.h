#pragma once
/**
 * @file MessageActions.h
 * @brief 右键菜单 action 逻辑（回复/复制/撤回/编辑/删除）
 * @details Phase 5B.6 从 ChatController.cpp 提取。
 */
#ifndef MESSAGEACTIONS_H
#define MESSAGEACTIONS_H

#include "ChatListModel.h"
#include "ProtocolStructs.h"
#include <QObject>
#include <QString>

class MessageActions : public QObject
{
    Q_OBJECT
public:
    explicit MessageActions(QObject *parent = nullptr);

    void setChatModel(ChatListModel *model);
    void setCurrentUid(int uid);

    Q_INVOKABLE void actionReply(qint64 timestamp);
    Q_INVOKABLE void actionCopyText(qint64 timestamp);
    Q_INVOKABLE void actionRecall(qint64 timestamp);
    Q_INVOKABLE void actionEdit(qint64 timestamp, const QString &newContent);
    Q_INVOKABLE void actionDelete(qint64 timestamp);

signals:
    void sigSendRecallMsg(const ChatRecallMsgStruct &msg);
    void sigSendEditMsg(const ChatEditMsgStruct &msg);
    void sigSetReplyContext(const QString &prefix);

private:
    ChatListModel *_chat_model = nullptr;
    int _current_uid = 0;
};

#endif // MESSAGEACTIONS_H
