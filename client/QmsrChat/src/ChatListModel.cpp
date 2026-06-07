/**
 * @file    ChatListModel.cpp
 * @brief   聊天消息列表模型实现（QAbstractListModel 子类，供 QML ListView 使用）
 */
#include "ChatListModel.h"
#include "DbWorker.h"
#include <QDateTime>
#include <QDebug>
#include <climits>

/**
 * @brief 构造函数
 * @param parent 父 QObject
 */
ChatListModel::ChatListModel(QObject *parent)
    : QAbstractListModel(parent),
      _current_uid(0)
{
}

/**
 * @brief 析构函数
 */
ChatListModel::~ChatListModel()
{
}

/**
 * @brief 获取模型行数（QAbstractListModel 接口）
 * @param parent 父索引（列表模型忽略）
 * @return 消息总数
 * @details 加锁读取 _messages，保证跨线程安全
 */
int ChatListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    QMutexLocker locker(&_mutex);
    return _messages.size();
}

/**
 * @brief 获取指定索引的数据（QAbstractListModel 接口）
 * @param index 模型索引
 * @param role 数据角色（对应 roleNames() 定义的属性名）
 * @return 对应角色的 QVariant 值
 * @details 加锁读取，跨线程安全。IsSelfRole 通过对比 from_uid 与 _current_uid 判断
 */
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

/**
 * @brief 返回 QML 角色名映射（QAbstractListModel 接口）
 * @return role int → QML 属性名字符串 的哈希表
 */
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

/**
 * @brief 添加一条消息到模型
 * @param msg 消息结构体
 * @details 按时间戳二分插入保持有序
 */
void ChatListModel::AddMessage(const ChatMessage &msg)
{
    InsertMessageSorted(msg);
}

/**
 * @brief 插入或更新消息（幂等 upsert）
 * @param msg 消息结构体
 * @details 优先通过 client_msg_id 哈希索引查找（O(1)），
 *          其次通过 server_msg_id 线性扫描（O(n)），
 *          找到则原地更新，未找到则二分插入
 */
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

/**
 * @brief 按时间戳二分查找插入位置并插入
 * @param msg 消息结构体
 * @details 插入后重建 _clientIdIndex 哈希索引，发射 scrollToBottomRequested 信号
 */
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

/**
 * @brief 在列表头部批量插入历史消息
 * @param messages 消息列表（时间升序）
 * @details 倒序遍历 prepend 保证插入后时间顺序正确，走 beginInsertRows/endInsertRows 通知 QML
 */
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

/**
 * @brief 更新消息状态并通知 QML
 * @param client_msg_id 客户端消息 ID
 * @param status 新状态（0=发送中，1=已送达，2=已存储，-1=失败）
 * @details 通过 _clientIdIndex 哈希索引 O(1) 查找，发射 dataChanged 仅通知 StatusRole
 */
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

/**
 * @brief 清空所有消息
 * @details 走 beginResetModel/endResetModel 通知 QML 整体刷新
 */
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

/**
 * @brief 设置当前登录用户 UID
 * @param uid 用户 ID
 * @details 用于 IsSelfRole 判断消息是否来自自己
 */
void ChatListModel::SetCurrentUid(int uid)
{
    QMutexLocker locker(&_mutex);
    _current_uid = uid;
}

/**
 * @brief 获取指定位置的消息（加锁拷贝）
 * @param index 消息在列表中的位置
 * @param out 输出参数，存储找到的消息
 * @return 是否获取成功
 */
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

/**
 * @brief 获取所有消息的拷贝
 * @return 消息列表（时间升序）
 * @details 返回拷贝而非引用，避免调用方持有锁期间意外阻塞
 */
QVector<ChatMessage> ChatListModel::GetAllMessages() const
{
    QMutexLocker locker(&_mutex);
    return _messages;
}

/**
 * @brief 获取最早消息的时间戳
 * @return 时间戳（列表为空返回 LLONG_MAX）
 * @details 用于 loadMoreHistory 计算 before_time 分页参数
 */
qint64 ChatListModel::GetEarliestTimestamp() const
{
    QMutexLocker locker(&_mutex);
    if (_messages.isEmpty())
    {
        return LLONG_MAX;
    }
    return _messages.first().timestamp;
}

/**
 * @brief 重建 client_msg_id → 行号 的哈希索引
 * @details 在 InsertMessageSorted 及 PrependMessages 后调用，
 *          保证 UpsertMessage 和 UpdateMessageStatus 的 O(1) 查找有效
 */
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

/**
 * @brief 格式化时间戳为显示字符串
 * @param timestamp 毫秒级时间戳
 * @return 格式化的时间字符串
 * @details 今天 → hh:mm:ss，昨天 → "昨天 hh:mm:ss"，今年 → MM-dd hh:mm:ss，跨年 → yyyy-MM-dd hh:mm:ss
 */
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

/**
 * @brief 按时间戳原地修改消息
 * @param ts 消息时间戳
 * @param mutator 修改函数（lambda），接收 ChatMessage& 引用
 * @details 加锁遍历并应用修改后解锁，再发射 dataChanged（避免持锁 emit）
 */
void ChatListModel::UpdateMessageByTimestamp(qint64 ts,
        const std::function<void(ChatMessage &)> &mutator)
{
    QMutexLocker lock(&_mutex);
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (_messages[i].timestamp == ts)
        {
            mutator(_messages[i]);
            lock.unlock();
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
            return;
        }
    }
}

/**
 * @brief 更新图片的本地路径
 * @param image_id 图片 UUID
 * @param local_path 本地文件路径
 * @details 文件下载完成后调用，通知 QML 更新 ImagePathRole
 */
void ChatListModel::UpdateImagePath(const QString &image_id, const QString &local_path)
{
    QMutexLocker lock(&_mutex);
    for (int i = 0; i < _messages.size(); ++i)
    {
        if (_messages[i].image_id == image_id)
        {
            _messages[i].image_path = local_path;
            lock.unlock();
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {ImagePathRole});
            return;
        }
    }
}

/**
 * @brief 标记消息为已撤回
 * @param ts 消息时间戳
 * @param current_uid 当前用户 ID（用于 DB 作用域）
 * @details 先异步写 DB 再改内存，防止刷新后 DB 中 recalled=0 导致消息"复活"
 */
void ChatListModel::MarkRecalled(qint64 ts, int current_uid)
{
    // 先写 DB（异步），再改内存。修复"假撤回"bug：刷新界面后 DB 仍是 recalled=0 导致图片复活
    DbThreadManager::Instance().MarkMessageRecalled(ts, current_uid);
    UpdateMessageByTimestamp(ts, [](ChatMessage &m) {
        m.recalled = true;
        m.recalled_at = QDateTime::currentMSecsSinceEpoch();
    });
}

/**
 * @brief 标记消息为已编辑
 * @param ts 消息时间戳
 * @param new_content 编辑后内容
 * @param edit_ts 编辑时间
 */
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

/**
 * @brief 按时间戳获取消息内容（便捷方法）
 * @param ts 消息时间戳
 * @return 消息内容字符串，未找到返回空
 */
QString ChatListModel::GetContentByTimestamp(qint64 ts) const
{
    ChatMessage m;
    if (GetMessageByTimestamp(ts, m)) return m.content;
    return QString();
}
