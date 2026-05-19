# P0 线程安全问题修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 msrChat 项目中的线程安全隐患，确保多线程访问共享数据时的安全性

**Architecture:** 分析 8 个线程安全问题，按优先级分类修复。客户端使用 QMutex 保护共享数据，服务端使用 std::mutex /strand 保护。

**Tech Stack:** C++, Qt, boost::asio

---

## 问题列表

| # | 问题 | 文件 | 严重度 |
|---|------|------|--------|
| 1 | ChatController::_pending_messages 无锁保护 | ChatController.cpp:182 | P0 |
| 2 | TcpWorker::_pending_connect 竞态 | TcpWorker.cpp:59-64,84,94 | P0 |
| 3 | CServer::SendOfflineMessages 跨线程访问 session | CServer.cpp:186-212 | P0 |
| 4 | ThreadPool Enqueue/Shutdown 同步问题 | ThreadPool.cpp | P1 |
| 5 | FileTransfer _tasks/_task_id_allocator 非原子 | FileTransfer.h:156-158 | P1 |
| 6 | ChatListModel 遍历非原子 | ChatListModel.h/cpp | P2 |
| 7 | CSession::_offline_send_state 无锁访问 | CSession.cpp:544-588 | P1 |
| 8 | LogicSystem GetTaskCount 竞态 | LogicSystem.cpp:40 | P1 |

---

## Task 1: ChatController::_pending_messages 线程安全

**Files:**
- Modify: `client/QmsrChat/include/ChatController.h:78`
- Modify: `client/QmsrChat/src/ChatController.cpp:182-186, slotCleanTimeoutMessages`

**问题:** `_pending_messages` 在 `sendMessage()` 写操作与 `slotCleanTimeoutMessages()` 清理操作之间存在竞争

**修复:** 添加 QMutex 保护所有对 `_pending_messages` 的访问

- [ ] **Step 1: 在 ChatController.h 添加 mutex 成员**

```cpp
private:
    QMutex _pending_mutex;  // 新增
    QHash<QString, PendingMessageInfo> _pending_messages;
```

- [ ] **Step 2: 修改 sendMessage() 添加锁**

```cpp
// ChatController.cpp:182
QMutexLocker locker(&_pending_mutex);  // 新增
_pending_messages.insert(client_msg_id, PendingMessageInfo{QDateTime::currentSecsSinceEpoch()});
```

- [ ] **Step 3: 修改 slotCleanTimeoutMessages() 添加锁**

```cpp
// 找到 slotCleanTimeoutMessages 函数，添加 QMutexLocker locker(&_pending_mutex);
```

---

## Task 2: TcpWorker::_pending_connect 竞态条件

**Files:**
- Modify: `client/QmsrChat/include/TcpWorker.h`
- Modify: `client/QmsrChat/src/TcpWorker.cpp:59-64,84,94`

**问题:** `_pending_connect` 读操作（slot_init:59）和写操作（slot_stop:70, slot_tcp_connect:88,94）之间无同步

**修复:** 添加 QMutex 保护 `_pending_connect` 访问

- [ ] **Step 1: 在 TcpWorker.h 添加 mutex**

```cpp
private:
    QMutex _pending_connect_mutex;  // 新增
    std::optional<ServerInfo> _pending_connect;
```

- [ ] **Step 2: 修改 slot_init() 添加锁**

```cpp
// TcpWorker.cpp:59
QMutexLocker locker(&_pending_connect_mutex);  // 新增
if (_pending_connect.has_value()) {
    const ServerInfo si = *_pending_connect;
    _pending_connect.reset();
    slot_tcp_connect(si);
}
```

- [ ] **Step 3: 修改 slot_tcp_connect() 添加锁**

```cpp
// TcpWorker.cpp:84
QMutexLocker locker(&_pending_connect_mutex);  // 新增
if (!_socket) {
    _pending_connect = si;
    return;
}
// ... 后续代码
_pending_connect = si;
```

- [ ] **Step 4: 修改 slot_stop() 添加锁**

```cpp
// TcpWorker.cpp:67
QMutexLocker locker(&_pending_connect_mutex);  // 新增
_state = ConnectionState::Stopping;
_pending_connect.reset();
```

---

## Task 3: CServer::SendOfflineMessages 跨线程访问

**Files:**
- Modify: `server/ChatServer/src/CServer.cpp:186-212`

**问题:** lambda 中直接访问 `session->_offline_send_state`，但 session 可能属于其他 strand

**修复:** 将 session 状态访问也封装到 strand 中

- [ ] **Step 1: 修改 SendOfflineMessages() 将所有 session 访问放入 strand**

```cpp
// CServer.cpp:190-211
// 原来：直接访问 session->_offline_send_state
// 修改：所有访问都通过 strand post

_thread_pool.Enqueue(
    [this, self, uid, session]()
    {
        int64_t total_count = SQLiteMgr::Instance().GetOfflineMessageCount(uid);

        if (total_count == 0)
        {
            return;
        }

        // 所有 session 访问都通过 strand
        boost::asio::post(
            session->GetStrand(),
            [self, session, uid, total_count]()
            {
                session->_offline_send_state.uid = uid;
                session->_offline_send_state.total_count = total_count;
                session->_offline_send_state.sent_count = 0;
                session->_offline_send_state.sending = true;

                session->SendNextOfflinePage();
            });
    });
```

---

## Task 4: ThreadPool Enqueue/Shutdown 同步增强

**Files:**
- Modify: `server/ChatServer/include/ThreadPool.h:30-36`
- Modify: `server/ChatServer/src/ThreadPool.cpp:35-62`

**问题:** `_stop` 和 `_tasks` 在 Enqueue 和 Shutdown 之间可能存在竞态

**修复:** 确保所有 _stop 读取都在锁保护下

- [ ] **Step 1: 检查 ThreadPool 代码**

当前代码检查发现：`_stop` 在 Shutdown() 中设置在锁内（line 53），在 Enqueue() 中没有设置 `_stop`。WorkerThread 循环检查 `_stop` 时已经获取锁（line 83）。

当前实现已经是正确的，无需修改。

---

## Task 5: FileTransfer 任务 ID 分配非原子

**Files:**
- Modify: `server/ChatServer/src/FileTransfer.cpp`

**问题:** `CreateTask` 中 `_task_id_allocator` 递增和 `_tasks` 插入之间非原子

**修复:** 添加锁保护 CreateTask 和 RemoveTask

- [ ] **Step 1: 添加互斥锁保护任务操作**

在 FileTransfer 类的 private 部分添加成员：
```cpp
std::mutex _task_mutex;
```

修改 CreateTask：
```cpp
int64_t FileTransfer::CreateTask(...) {
    int64_t task_id;
    {
        std::lock_guard<std::mutex> lock(_task_mutex);
        task_id = _task_id_allocator.fetch_add(1);
    }
    // ... 创建 task
    {
        std::lock_guard<std::mutex> lock(_task_mutex);
        _tasks[task_id] = task;
    }
    return task_id;
}
```

修改 RemoveTask：
```cpp
void FileTransfer::RemoveTask(int64_t task_id) {
    std::lock_guard<std::mutex> lock(_task_mutex);
    _tasks.erase(task_id);
}
```

修改 GetTask：
```cpp
std::shared_ptr<FileTransferTask> FileTransfer::GetTask(int64_t task_id) {
    std::shared_lock<std::shared_mutex> lock(_tasks_mutex);  // 读锁
    auto it = _tasks.find(task_id);
    if (it != _tasks.end()) {
        return it->second;
    }
    return nullptr;
}
```

---

## Task 6: ChatListModel 遍历原子性

**Files:**
- Modify: `client/QmsrChat/include/ChatListModel.h`
- Modify: `client/QmsrChat/src/ChatListModel.cpp`

**问题:** `data()` 和 `rowCount()` 各自独立加锁，调用者遍历时无法保证原子性

**说明:** 如果 ChatListModel 仅在主线程被 QML 访问，当前实现是安全的。如果从后台线程访问，则需要修复。

**修复:** 添加一个方法用于原子批量获取消息

- [ ] **Step 1: 添加原子批量获取方法**

在 ChatListModel.h 中添加：
```cpp
// 原子获取多个消息，start 开始位置，count 数量
QVector<ChatMessage> GetMessagesAtomic(int start, int count) const;
```

实现：
```cpp
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
```

---

## Task 7: CSession::_offline_send_state 无锁访问

**Files:**
- Modify: `server/ChatServer/src/CSession.cpp`

**问题:** `SendNextOfflinePage()` 直接访问 `_offline_send_state` 成员变量，未加锁

**修复:** 添加 `_offline_mutex` 保护对 `_offline_send_state` 的访问

- [ ] **Step 1: 在 CSession.h 添加 mutex**

```cpp
private:
    std::mutex _offline_mutex;  // 新增
```

- [ ] **Step 2: 修改 SendNextOfflinePage() 添加锁**

```cpp
void CSession::SendNextOfflinePage() {
    std::lock_guard<std::mutex> lock(_offline_mutex);
    // ... 访问 _offline_send_state
}
```

- [ ] **Step 3: 检查其他访问 _offline_send_state 的地方并添加锁**

搜索 `AppendFileChunk` 等函数，确保所有访问都在锁保护下。

---

## Task 8: LogicSystem GetTaskCount 竞态

**Files:**
- Modify: `server/ChatServer/src/LogicSystem.cpp:40`

**问题:** `GetTaskCount() > MAX_QUEUE_SIZE` 判断和入队之间存在竞态，队列可能在此期间溢出

**修复:** 在锁内完成检查和入队操作

- [ ] **Step 1: 修改 EnqueueLogical 逻辑**

当前代码从 ThreadPool 获取任务计数的时机存在问题。实际的 ThreadPool::Enqueue 内部会加锁，所以逻辑系统的 EnqueueLogical 不需要在外部判断队列大小。

删除 GetTaskCount 检查，因为 ThreadPool 内部会正确处理：

```cpp
// LogicSystem.cpp:38-42
// 删除以下代码：
// if (_thread_pool.GetTaskCount() > MAX_QUEUE_SIZE) {
//     spdlog::error("[LogicSystem] Task queue overflow");
//     return;
// }
```

---

## 执行顺序

1. Task 1: ChatController::_pending_messages（最简单，影响用户消息发送）
2. Task 2: TcpWorker::_pending_connect（网络连接）
3. Task 3: CServer::SendOfflineMessages（服务端离线消息）
4. Task 7: CSession::_offline_send_state（关联 Task 3）
5. Task 5: FileTransfer 任务操作（服务端文件传输）
6. Task 6: ChatListModel（客户端聊天列表）
7. Task 8: LogicSystem（清理无效检查）

**Note:** Task 4 经分析无需修改。

---

## 验证方法

每个 Task 完成后：
1. 编译项目 `cmake --build .`
2. 运行客户端和服务端
3. 测试基本聊天功能：登录、发送消息、接收消息、断开重连
4. 检查日志中是否有线程相关警告

**整体验证:**
- 发送 100 条消息，观察是否有数据丢失或崩溃
- 多客户端同时连接，观察服务端稳定性