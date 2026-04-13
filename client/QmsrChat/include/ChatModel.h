#ifndef CHAT_MODEL_H
#define CHAT_MODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QModelIndex>
#include <QVariant>
#include "DbMgr.h"

class ChatModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum ChatRoles {
        FromUidRole = Qt::UserRole + 1,
        ToUidRole,
        ContentRole,
        TimestampRole,
        StatusRole,
        IsSelfRole
    };
    
    enum ChatColumns {
        ColumnTime = 0,
        ColumnContent,
        ColumnStatus,
        ColumnCount
    };
    
    explicit ChatModel(QObject* parent = nullptr);
    
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void AddMessage(const ChatMessage& msg);
    void AddMessages(const QVector<ChatMessage>& messages);
    void LoadMessages(int uid1, int uid2);
    void SearchMessages(int uid1, int uid2, const QString& keyword);
    void ClearMessages();
    
    const QVector<ChatMessage>& GetMessages() const { return _messages; }
    
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void messagesLoaded(int count);
    void messageAdded(const ChatMessage& msg);

private:
    QVector<ChatMessage> _messages;
    int _current_uid;
    int _chat_uid;
};

#endif
