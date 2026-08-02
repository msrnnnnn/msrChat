/**
 * @file MessageActions.cpp
 * @brief 右键菜单 action 实现
 * @details Phase 5B.6 从 ChatController.cpp 提取。
 */
#include "MessageActions.h"
#include "DbWorker.h"
#include <QClipboard>
#include <QGuiApplication>

MessageActions::MessageActions(QObject *parent) : QObject(parent)
{
}

void MessageActions::setChatModel(ChatListModel *model)
{
    _chat_model = model;
}

void MessageActions::setCurrentUid(int uid)
{
    _current_uid = uid;
}

/**
 * @brief 菜单项：回复（v1 stub）
 * @details 在输入框插入"回复 {from_uid}: "前缀
 */
void MessageActions::actionReply(qint64 timestamp)
{
    if (_chat_model == nullptr)
        return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp)
        {
            const QString prefix = QStringLiteral("回复 %1: ").arg(m.from_uid);
            emit sigSetReplyContext(prefix);
            return;
        }
    }
}

/**
 * @brief 菜单项：复制文字（走系统剪贴板）
 */
void MessageActions::actionCopyText(qint64 timestamp)
{
    if (_chat_model == nullptr)
        return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp)
        {
            QGuiApplication::clipboard()->setText(m.content);
            return;
        }
    }
}

/**
 * @brief 菜单项：撤回（仅自方 + 2 分钟内）
 */
void MessageActions::actionRecall(qint64 timestamp)
{
    if (_chat_model == nullptr)
        return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp && m.from_uid == _current_uid)
        {
            ChatRecallMsgStruct req;
            req.from_uid = _current_uid;
            req.msg_timestamp = timestamp;
            req.client_msg_id = m.client_msg_id;
            emit sigSendRecallMsg(req);
            return;
        }
    }
}

/**
 * @brief 菜单项：编辑（仅自方，长度校验本地做）
 */
void MessageActions::actionEdit(qint64 timestamp, const QString &newContent)
{
    if (newContent.isEmpty() || newContent.size() > 4096)
        return;
    if (_chat_model == nullptr)
        return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp && m.from_uid == _current_uid)
        {
            ChatEditMsgStruct req;
            req.from_uid = _current_uid;
            req.msg_timestamp = timestamp;
            req.new_content = newContent;
            emit sigSendEditMsg(req);
            return;
        }
    }
}

/**
 * @brief 菜单项：删除（仅本地，不通知对端）
 */
void MessageActions::actionDelete(qint64 timestamp)
{
    if (_chat_model == nullptr)
        return;
    _chat_model->RemoveMessageByTimestamp(timestamp);
    DbThreadManager::Instance().DeleteMessageByTimestamp(timestamp);
}
