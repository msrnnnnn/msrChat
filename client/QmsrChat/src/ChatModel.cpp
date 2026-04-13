#include "ChatModel.h"
#include <QDateTime>
#include <QDebug>

ChatModel::ChatModel(QObject* parent)
    : QAbstractTableModel(parent)
    , _current_uid(0)
    , _chat_uid(0)
{
}

int ChatModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return _messages.size();
}

int ChatModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ChatModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    
    int row = index.row();
    if (row < 0 || row >= _messages.size()) {
        return QVariant();
    }
    
    const ChatMessage& msg = _messages[row];
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ColumnTime:
                return QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString("hh:mm:ss");
            case ColumnContent:
                return msg.content;
            case ColumnStatus:
                switch (msg.status) {
                    case 0: return QString("Sending");
                    case 1: return QString("Sent");
                    case 2: return QString("Read");
                    default: return QString("Unknown");
                }
            default:
                return QVariant();
        }
    }
    
    if (role == FromUidRole) {
        return msg.from_uid;
    }
    if (role == ToUidRole) {
        return msg.to_uid;
    }
    if (role == ContentRole) {
        return msg.content;
    }
    if (role == TimestampRole) {
        return msg.timestamp;
    }
    if (role == StatusRole) {
        return msg.status;
    }
    if (role == IsSelfRole) {
        return msg.from_uid == _current_uid;
    }
    
    return QVariant();
}

QVariant ChatModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    
    if (orientation == Qt::Horizontal) {
        switch (section) {
            case ColumnTime:
                return QString("Time");
            case ColumnContent:
                return QString("Content");
            case ColumnStatus:
                return QString("Status");
            default:
                return QVariant();
        }
    }
    
    return QVariant();
}

void ChatModel::AddMessage(const ChatMessage& msg)
{
    beginInsertRows(QModelIndex(), _messages.size(), _messages.size());
    _messages.append(msg);
    endInsertRows();
    
    emit messageAdded(msg);
}

void ChatModel::AddMessages(const QVector<ChatMessage>& messages)
{
    if (messages.isEmpty()) {
        return;
    }
    
    beginInsertRows(QModelIndex(), 0, messages.size() - 1);
    
    for (int i = messages.size() - 1; i >= 0; --i) {
        _messages.prepend(messages[i]);
    }
    
    endInsertRows();
    
    emit messagesLoaded(messages.size());
}

void ChatModel::LoadMessages(int uid1, int uid2)
{
    _current_uid = uid1;
    _chat_uid = uid2;
    
    QVector<ChatMessage> messages = DbMgr::Instance().GetMessages(uid1, uid2);
    
    beginResetModel();
    _messages.clear();
    
    for (int i = messages.size() - 1; i >= 0; --i) {
        _messages.append(messages[i]);
    }
    
    endResetModel();
    
    emit messagesLoaded(_messages.size());
}

void ChatModel::SearchMessages(int uid1, int uid2, const QString& keyword)
{
    _current_uid = uid1;
    _chat_uid = uid2;
    
    QVector<ChatMessage> messages = DbMgr::Instance().SearchMessages(uid1, uid2, keyword);
    
    beginResetModel();
    _messages.clear();
    
    for (int i = messages.size() - 1; i >= 0; --i) {
        _messages.append(messages[i]);
    }
    
    endResetModel();
    
    emit messagesLoaded(_messages.size());
}

void ChatModel::ClearMessages()
{
    beginResetModel();
    _messages.clear();
    endResetModel();
}

Qt::ItemFlags ChatModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
