#include "ChatListModel.h"
#include "DbWorker.h"
#include <QDateTime>
#include <QDebug>
#include <climits>
#include <cstdio>
#include <ctime>

ChatListModel::ChatListModel(QObject *parent)
    : QAbstractListModel(parent),
      _current_uid(0)
{
}

ChatListModel::~ChatListModel()
{
}

int ChatListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    QMutexLocker locker(&_mutex);
    return _messages.size();
}

QVariant ChatListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    QMutexLocker locker(&_mutex);

    int row = index.row();
    if (row < 0 || row >= _messages.size())
    {
        return QVariant();
    }

    const ChatMessage &msg = _messages[row];

    switch (role)
    {
        case FromUidRole:
            return msg.from_uid;
        case ToUidRole:
            return msg.to_uid;
        case ContentRole:
            return msg.content;
        case TimestampRole:
            return msg.timestamp;
        case StatusRole:
            return msg.status;
        case IsSelfRole:
            return msg.from_uid == _current_uid;
        case DisplayTimeRole:
            return FormatTime(msg.timestamp);
        case TypeRole:
            return msg.type;
        case ImageIdRole:
            return msg.image_id;
        case ImagePathRole:
            return msg.image_path;
        case ImageWidthRole:
            return msg.image_width;
        case ImageHeightRole:
            return msg.image_height;
        case EditedRole:
            return msg.edited;
        case RecalledRole:
            return msg.recalled;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FromUidRole] = "fromUid";
    roles[ToUidRole] = "toUid";
    roles[ContentRole] = "content";
    roles[TimestampRole] = "timestamp";
    roles[StatusRole] = "status";
    roles[IsSelfRole] = "isSelf";
    roles[DisplayTimeRole] = "displayTime";
    roles[TypeRole] = "messageType";
    roles[ImageIdRole] = "imageId";
    roles[ImagePathRole] = "imagePath";
    roles[ImageWidthRole] = "imageWidth";
    roles[ImageHeightRole] = "imageHeight";
    roles[EditedRole] = "edited";
    roles[RecalledRole] = "recalled";
    return roles;
}

void ChatListModel::AddMessage(const ChatMessage &msg)
{
    InsertMessageSorted(msg);
}

void ChatListModel::UpsertMessage(const ChatMessage &msg)
{
    int changedRow = -1;
    {
        QMutexLocker locker(&_mutex);

        // O(1) hash lookup by client_msg_id
        if (!msg.client_msg_id.isEmpty())
        {
            auto it = _clientIdIndex.find(msg.client_msg_id);
            if (it != _clientIdIndex.end() && it.value() >= 0 && it.value() < _messages.size())
            {
                if (_messages[it.value()].client_msg_id == msg.client_msg_id)
                {
                    changedRow = it.value();
                }
            }
        }

        // Fall back to linear scan for server_msg_id match
        if (changedRow < 0)
        {
            for (int i = 0; i < _messages.size(); ++i)
            {
                const bool sameServerId = msg.server_msg_id > 0 && _messages[i].server_msg_id == msg.server_msg_id;
                if (sameServerId)
                {
                    changedRow = i;
                    break;
                }
            }
        }

        if (changedRow >= 0)
        {
            ChatMessage copy = msg;
            _messages[changedRow] = copy;
            if (!copy.client_msg_id.isEmpty())
            {
                _clientIdIndex[copy.client_msg_id] = changedRow;
            }
        }
    }

    if (changedRow >= 0)
    {
        const QModelIndex idx = index(changedRow, 0);
        emit dataChanged(idx, idx);
        return;
    }

    InsertMessageSorted(msg);
}

void ChatListModel::InsertMessageSorted(const ChatMessage &msg)
{
    ChatMessage copy = msg;

    int insertRow = 0;
    {
        int lo = 0, hi = _messages.size();
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            if (_messages[mid].timestamp <= msg.timestamp)
                lo = mid + 1;
            else
                hi = mid;
        }
        insertRow = lo;
    }

    beginInsertRows(QModelIndex(), insertRow, insertRow);
    _messages.insert(insertRow, copy);
    RebuildIndex();
    endInsertRows();

    emit messageAdded(copy);
    emit scrollToBottomRequested();
}

void ChatListModel::PrependMessages(const QVector<ChatMessage> &messages)
{
    if (messages.isEmpty())
    {
        return;
    }

    int startRow = 0;
    int endRow = messages.size() - 1;

    beginInsertRows(QModelIndex(), startRow, endRow);

    {
        QMutexLocker locker(&_mutex);
        for (int i = messages.size() - 1; i >= 0; --i)
        {
            ChatMessage copy = messages[i];
            _messages.prepend(copy);
        }
        RebuildIndex();
    }

    endInsertRows();
}

void ChatListModel::UpdateMessageStatus(const QString &client_msg_id, int status)
{
    int changedRow = -1;

    {
        QMutexLocker locker(&_mutex);
        auto it = _clientIdIndex.find(client_msg_id);
        if (it != _clientIdIndex.end() && it.value() >= 0 && it.value() < _messages.size()
            && _messages[it.value()].client_msg_id == client_msg_id)
        {
            _messages[it.value()].status = status;
            changedRow = it.value();
        }
    }

    if (changedRow >= 0)
    {
        QModelIndex changedIndex = index(changedRow, 0);
        emit dataChanged(changedIndex, changedIndex, {StatusRole});
    }
}

void ChatListModel::ClearMessages()
{
    beginResetModel();

    {
        QMutexLocker locker(&_mutex);
        _messages.clear();
        _clientIdIndex.clear();
    }

    endResetModel();
}

void ChatListModel::SetCurrentUid(int uid)
{
    QMutexLocker locker(&_mutex);
    _current_uid = uid;
}

bool ChatListModel::TryGetMessageAt(int index, ChatMessage &out) const
{
    QMutexLocker locker(&_mutex);

    if (index < 0 || index >= _messages.size())
    {
        return false;
    }

    out = _messages[index];
    return true;
}

QVector<ChatMessage> ChatListModel::GetAllMessages() const
{
    QMutexLocker locker(&_mutex);
    return _messages;
}

QVector<ChatMessage> ChatListModel::GetMessagesAtomic(int start, int count) const
{
    QMutexLocker locker(&_mutex);
    QVector<ChatMessage> result;
    int end = qMin(start + count, _messages.size());
    for (int i = start; i < end; ++i) {
        result.append(_messages[i]);
    }
    return result;
}

qint64 ChatListModel::GetEarliestTimestamp() const
{
    QMutexLocker locker(&_mutex);
    if (_messages.isEmpty())
    {
        return LLONG_MAX;
    }
    return _messages.first().timestamp;
}

void ChatListModel::RebuildIndex()
{
    _clientIdIndex.clear();
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (!_messages[i].client_msg_id.isEmpty())
        {
            _clientIdIndex[_messages[i].client_msg_id] = i;
        }
    }
}

QString ChatListModel::FormatTime(qint64 timestamp) const
{
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    QDateTime now = QDateTime::currentDateTime();

    if (dateTime.date() == now.date())
    {
        return dateTime.toString("hh:mm:ss");
    }
    else if (dateTime.date().daysTo(now.date()) == 1)
    {
        return QString("昨天 %1").arg(dateTime.toString("hh:mm:ss"));
    }
    else if (dateTime.date().year() == now.date().year())
    {
        return dateTime.toString("MM-dd hh:mm:ss");
    }
    else
    {
        return dateTime.toString("yyyy-MM-dd hh:mm:ss");
    }
}

void ChatListModel::UpdateMessageByTimestamp(qint64 ts,
        const std::function<void(ChatMessage &)> &mutator)
{
    QMutexLocker lock(&_mutex);
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (_messages[i].timestamp == ts)
        {
            mutator(_messages[i]);
            // 局部刷新：dataChanged 而非 beginResetModel/endResetModel。
            // beginResetModel 在 ListView + Loader delegate 组合下会破坏 Loader 内部状态
            // （撤回后图片"藏在 ListView 下方、聊几句后冒出来" 的根因之一）。
            lock.unlock();
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
            return;
        }
    }
}

void ChatListModel::UpdateImagePath(const QString &image_id, const QString &local_path)
{
    QMutexLocker lock(&_mutex);
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (_messages[i].image_id == image_id)
        {
            _messages[i].image_path = local_path;
            lock.unlock();
            beginResetModel();
            endResetModel();
            return;
        }
    }
}

void ChatListModel::MarkRecalled(qint64 ts, int current_uid)
{
    // 先写 DB（异步），再改内存。修复"假撤回"bug：刷新界面后 DB 仍是 recalled=0 导致图片复活
    DbThreadManager::Instance().MarkMessageRecalled(ts, current_uid);
    UpdateMessageByTimestamp(ts, [](ChatMessage &m) {
        m.recalled = true;
        m.recalled_at = QDateTime::currentMSecsSinceEpoch();
    });
}

void ChatListModel::MarkEdited(qint64 ts, const QString &new_content, qint64 edit_ts)
{
    UpdateMessageByTimestamp(ts, [&](ChatMessage &m) {
        m.content = new_content;
        m.edited = true;
        m.edited_at = edit_ts;
    });
}

// === Phase 6 ===

/**
 * @brief 按时间戳删除单条消息（仅本地）
 * @details 走 beginRemoveRows / endRemoveRows 让 QML ListView 正确更新。
 *          不通知对端（删除作用域：本地）。
 */
void ChatListModel::RemoveMessageByTimestamp(qint64 ts)
{
    QMutexLocker lock(&_mutex);
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (_messages[i].timestamp == ts)
        {
            beginRemoveRows(QModelIndex(), i, i);
            _messages.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

/**
 * @brief 按时间戳查找消息
 * @param ts 消息时间戳（毫秒）
 * @param out 找到时填充
 * @return true 找到；false 未找到
 */
bool ChatListModel::GetMessageByTimestamp(qint64 ts, ChatMessage &out) const
{
    QMutexLocker lock(&_mutex);
    for (const auto &m : _messages)
    {
        if (m.timestamp == ts)
        {
            out = m;
            return true;
        }
    }
    return false;
}

QString ChatListModel::GetContentByTimestamp(qint64 ts) const
{
    ChatMessage m;
    if (GetMessageByTimestamp(ts, m)) return m.content;
    return QString();
}
