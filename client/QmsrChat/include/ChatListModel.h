/**
 * @file ChatListModel.h
 * @brief 聊天消息列表数据模型
 * @details 基于 QAbstractListModel，为 QML ListView 提供消息数据，支持增删改查、排序插入和分页加载。
 */
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

/**
 * @brief 聊天消息的 QAbstractListModel 实现，为 QML ListView 提供数据源
 * @details 维护消息列表和 client_msg_id 索引表，支持消息添加、更新、排序、分页和撤回/编辑操作，线程安全。
 */
class ChatListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief QML 数据角色枚举，对应每条消息的各个字段
     */
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

    /**
     * @brief 追加消息到列表末尾
     */
    void AddMessage(const ChatMessage &msg);
    /**
     * @brief 根据 client_msg_id 插入或更新消息（消息去重）
     */
    void UpsertMessage(const ChatMessage &msg);
    /**
     * @brief 按时间戳升序插入消息，维护列表时序
     */
    void InsertMessageSorted(const ChatMessage &msg);
    /**
     * @brief 在列表头部批量插入历史消息（分页加载）
     */
    void PrependMessages(const QVector<ChatMessage> &messages);
    /**
     * @brief 根据客户端消息 ID 更新消息发送状态
     */
    void UpdateMessageStatus(const QString &client_msg_id, int status);
    /**
     * @brief 清空所有消息
     */
    void ClearMessages();
    /**
     * @brief 设置当前登录用户 uid，影响 IsSelfRole 的计算
     */
    void SetCurrentUid(int uid);
    /**
     * @brief 重建 client_msg_id 到行号的索引哈希表
     */
    void RebuildIndex();

    /**
     * @brief 按索引获取消息（线程安全）
     * @param index 消息在列表中的位置
     * @param[out] out 输出消息引用
     * @return true 表示索引有效，false 表示越界
     */
    bool TryGetMessageAt(int index, ChatMessage &out) const;
    QVector<ChatMessage> GetAllMessages() const;
    /**
     * @brief 获取列表中最早消息的时间戳，用于分页加载锚点
     */
    qint64 GetEarliestTimestamp() const;
    /**
     * @brief 按时间戳查找并修改消息
     * @param ts 消息时间戳
     * @param mutator 修改回调（lambda）
     */
    void UpdateMessageByTimestamp(qint64 ts, const std::function<void(ChatMessage &)> &mutator);
    /**
     * @brief 标记某条消息为已撤回
     * @param ts 消息时间戳
     * @param current_uid 当前用户 uid（用于区分撤回方）
     */
    void MarkRecalled(qint64 ts, int current_uid);
    /**
     * @brief 标记某条消息为已编辑，更新内容和编辑时间
     */
    void MarkEdited(qint64 ts, const QString &new_content, qint64 edit_ts);
    // Phase 6 — 单条删除 / 查询
    /**
     * @brief 按时间戳删除单条消息
     */
    void RemoveMessageByTimestamp(qint64 ts);
    /**
     * @brief 按时间戳查找消息（线程安全）
     * @param[out] out 输出消息引用
     * @return true 表示找到
     */
    bool GetMessageByTimestamp(qint64 ts, ChatMessage &out) const;
    /**
     * @brief 按时间戳获取消息内容文本（供 QML 调用）
     */
    Q_INVOKABLE QString GetContentByTimestamp(qint64 ts) const;
    /**
     * @brief 根据图片 image_id 更新本地缓存路径
     */
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

    /**
     * @brief 将时间戳格式化为 QML 展示用的时间字符串
     */
    QString FormatTime(qint64 timestamp) const;
};

#endif
