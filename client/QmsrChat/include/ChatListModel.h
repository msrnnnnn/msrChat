#ifndef CHATLISTMODEL_H
#define CHATLISTMODEL_H

#include "DbService.h"
#include <QAbstractListModel>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <functional>
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
        TypeRole,
        ImageIdRole,
        ImagePathRole,
        ImageWidthRole,
        ImageHeightRole,
        EditedRole,
        RecalledRole
    };

    explicit ChatListModel(QObject *parent = nullptr);
    ~ChatListModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void AddMessage(const ChatMessage &msg);
    void UpsertMessage(const ChatMessage &msg);
    void InsertMessageSorted(const ChatMessage &msg);
    void PrependMessages(const QVector<ChatMessage> &messages);
    void UpdateMessageStatus(const QString &client_msg_id, int status);
    void ClearMessages();
    void SetCurrentUid(int uid);
    void RebuildIndex();

    bool TryGetMessageAt(int index, ChatMessage &out) const;
    QVector<ChatMessage> GetAllMessages() const;
    QVector<ChatMessage> GetMessagesAtomic(int start, int count) const;
    qint64 GetEarliestTimestamp() const;
    void UpdateMessageByTimestamp(qint64 ts, const std::function<void(ChatMessage &)> &mutator);
    void MarkRecalled(qint64 ts);
    void MarkEdited(qint64 ts, const QString &new_content, qint64 edit_ts);
    // Phase 6 — 单条删除 / 查询
    void RemoveMessageByTimestamp(qint64 ts);
    bool GetMessageByTimestamp(qint64 ts, ChatMessage &out) const;
    Q_INVOKABLE QString GetContentByTimestamp(qint64 ts) const;
    void UpdateImagePath(const QString &image_id, const QString &local_path);

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
};

#endif
