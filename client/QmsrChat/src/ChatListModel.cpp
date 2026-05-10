#include "ChatListModel.h"
#include <QDateTime>
#include <QDebug>

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
        case BubbleWidthRole:
            return msg.bubbleWidth;
        case BubbleHeightRole:
            return msg.bubbleHeight;
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
    roles[BubbleWidthRole] = "bubbleWidth";
    roles[BubbleHeightRole] = "bubbleHeight";
    return roles;
}

void ChatListModel::AddMessage(const ChatMessage &msg)
{
    int lastRow = rowCount();
    beginInsertRows(QModelIndex(), lastRow, lastRow);

    {
        QMutexLocker locker(&_mutex);
        ChatMessage copy = msg;
        copy.bubbleWidth = CalculateBubbleWidth(copy.content);
        copy.bubbleHeight = CalculateBubbleHeight(copy.content);
        _messages.append(copy);
        if (!copy.client_msg_id.isEmpty())
        {
            _clientIdIndex[copy.client_msg_id] = _messages.size() - 1;
        }
    }

    endInsertRows();
    emit messageAdded(msg);
    emit scrollToBottomRequested();
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
            copy.bubbleWidth = CalculateBubbleWidth(copy.content);
            copy.bubbleHeight = CalculateBubbleHeight(copy.content);
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
        emit scrollToBottomRequested();
        return;
    }

    AddMessage(msg);
}

void ChatListModel::AddMessages(const QVector<ChatMessage> &messages)
{
    if (messages.isEmpty())
    {
        return;
    }

    int startRow = rowCount();
    int endRow = startRow + messages.size() - 1;

    beginInsertRows(QModelIndex(), startRow, endRow);

    {
        QMutexLocker locker(&_mutex);
        for (const ChatMessage &msg : messages)
        {
            ChatMessage copy = msg;
            copy.bubbleWidth = CalculateBubbleWidth(copy.content);
            copy.bubbleHeight = CalculateBubbleHeight(copy.content);
            _messages.append(copy);
        }
        RebuildIndex();
    }

    endInsertRows();

    emit messagesLoaded(messages.size());
    emit scrollToBottomRequested();
}

void ChatListModel::InsertHistoricalMessages(const QVector<ChatMessage> &messages)
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
            copy.bubbleWidth = CalculateBubbleWidth(copy.content);
            copy.bubbleHeight = CalculateBubbleHeight(copy.content);
            _messages.prepend(copy);
        }
        RebuildIndex();
    }

    endInsertRows();

    emit messagesLoaded(messages.size());
    emit scrollToTopRequested();
}

void ChatListModel::SetMessages(const QVector<ChatMessage> &messages)
{
    beginResetModel();
    {
        QMutexLocker locker(&_mutex);
        _messages = messages;
        RebuildIndex();
    }
    endResetModel();
    emit scrollToBottomRequested();
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

int ChatListModel::CalculateBubbleWidth(const QString &content) const
{
    int charCount = content.length();
    int baseWidth = 80;
    int maxWidth = 280;

    if (charCount <= 10)
    {
        return baseWidth + charCount * 6;
    }
    else if (charCount <= 30)
    {
        return baseWidth + 60 + (charCount - 10) * 5;
    }
    else
    {
        int width = baseWidth + 60 + 100 + (charCount - 30) * 4;
        return qMin(width, maxWidth);
    }
}

int ChatListModel::CalculateBubbleHeight(const QString &content) const
{
    int charCount = content.length();
    int baseHeight = 40;
    int lineHeight = 25;
    int charsPerLine = 25;

    int lines = (charCount + charsPerLine - 1) / charsPerLine;
    return baseHeight + (lines - 1) * lineHeight;
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
