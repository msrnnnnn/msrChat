#ifndef CHATLISTMODEL_H
#define CHATLISTMODEL_H

#include "DbMgr.h"
#include <QAbstractListModel>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>

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
    void AddMessages(const QVector<ChatMessage> &messages);
    void InsertHistoricalMessages(const QVector<ChatMessage> &messages);
    void UpdateMessageStatus(qint64 msg_id, int status);
    void ClearMessages();
    void SetCurrentUid(int uid);

    const ChatMessage &GetMessageAt(int index) const;
    QVector<ChatMessage> GetAllMessages() const;

signals:
    void messageAdded(const ChatMessage &msg);
    void messagesLoaded(int count);
    void scrollToBottomRequested();
    void scrollToTopRequested();

private:
    QVector<ChatMessage> _messages;
    mutable QMutex _mutex;
    int _current_uid;

    QString FormatTime(qint64 timestamp) const;
    int CalculateBubbleWidth(const QString &content) const;
    int CalculateBubbleHeight(const QString &content) const;
};

#endif
