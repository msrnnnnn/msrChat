#ifndef CHATLISTMODEL_H
#define CHATLISTMODEL_H

#include "DbMgr.h"
#include <QAbstractListModel>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <optional>

class ChatListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ChatRoles
    {
        FromUidRole = Qt::UserRole + 1,
        ToUidRole,
        ContentRole,
        TimestampRole,
        StatusRole,
        IsSelfRole,
        DisplayTimeRole,
        BubbleWidthRole,
        BubbleHeightRole
    };

    explicit ChatListModel(QObject *parent = nullptr);
    ~ChatListModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void AddMessage(const ChatMessage &msg);
    void UpsertMessage(const ChatMessage &msg);
    void AddMessages(const QVector<ChatMessage> &messages);
    void InsertHistoricalMessages(const QVector<ChatMessage> &messages);
    void SetMessages(const QVector<ChatMessage> &messages);
    void UpdateMessageStatus(const QString &client_msg_id, int status);
    void ClearMessages();
    void SetCurrentUid(int uid);
    void RebuildIndex();

    bool TryGetMessageAt(int index, ChatMessage &out) const;
    QVector<ChatMessage> GetAllMessages() const;
    QVector<ChatMessage> GetMessagesAtomic(int start, int count) const;

signals:
    void messageAdded(const ChatMessage &msg);
    void messagesLoaded(int count);
    void scrollToBottomRequested();
    void scrollToTopRequested();

private:
    QVector<ChatMessage> _messages;
    QHash<QString, int> _clientIdIndex;
    mutable QMutex _mutex;
    int _current_uid;

    QString FormatTime(qint64 timestamp) const;
    int CalculateBubbleWidth(const QString &content) const;
    int CalculateBubbleHeight(const QString &content) const;
};

#endif
