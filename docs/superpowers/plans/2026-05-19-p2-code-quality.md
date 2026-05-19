# P2 代码质量问题修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 P2 级别的代码质量问题（注释、魔法数字、God Class）

**Architecture:** 分析 3 个 P2 问题，按优先级分类修复。注释问题补充文档，魔法数字提取为常量，God Class 考虑拆分但需谨慎。

**Tech Stack:** C++, Qt, boost::asio

---

## 问题列表

| # | 问题 | 文件 | 严重度 |
|---|------|------|--------|
| 1 | 注释问题 - 关键函数缺少文档 | 多个文件 | P2 |
| 2 | 魔法数字 - 应提取为常量 | TcpWorker.cpp 等 | P2 |
| 3 | God Class - CSession 职责过多 | CSession.h:319-574 | P2 |

---

## Task 1: 补充关键函数注释

**Files:**
- Modify: `server/ChatServer/src/CSession.cpp:340-345`
- Modify: `client/QmsrChat/src/TcpWorker.cpp:314-360`
- Modify: `server/ChatServer/src/MessageDispatcher.cpp:53-499`

**问题:** 重要方法缺少参数和返回值说明

**修复:** 按已有风格（中文注释）补充文档

- [ ] **Step 1: 为 CSession 的 Send/SendBinary 方法添加注释**

```cpp
/**
 * @brief 发送消息到对端
 * @param msg 消息内容
 * @param msg_id 消息类型 ID
 * @return 发送是否成功
 */
bool Send(const std::string &msg, short msg_id);
```

- [ ] **Step 2: 为 TcpWorker 的 readBytes/schedule_reconnect 等方法添加注释**

- [ ] **Step 3: 为 MessageDispatcher 的 HandleXXXRequest 方法添加注释**

---

## Task 2: 提取魔法数字为常量

**Files:**
- Modify: `client/QmsrChat/src/TcpWorker.cpp`
- Modify: `server/ChatServer/include/const.h`

**问题:** 代码中存在多处魔法数字

**修复:** 提取为命名常量

- [ ] **Step 1: 提取 TcpWorker.cpp 中的魔法数字**

```cpp
// 在 TcpWorker.cpp 顶部或头文件中添加
constexpr int HEARTBEAT_INTERVAL_MS = 15000;
constexpr int PONG_CHECK_INTERVAL_MS = 5000;
constexpr int CONNECTION_TIMEOUT_MS = 45000;
constexpr int MAX_RECONNECT_INTERVAL_MS = 60000;
constexpr size_t RECV_BUFFER_SIZE = 2 * 1024 * 1024;  // 2MB
```

- [ ] **Step 2: 替换所有魔法数字为常量**

搜索并替换:
- `_recv_buffer(RingBuffer(2 * 1024 * 1024))` → `RingBuffer(RECV_BUFFER_SIZE)`
- `15000` → `HEARTBEAT_INTERVAL_MS`
- 等

---

## Task 3: CSession God Class 分析

**Files:**
- Read: `server/ChatServer/include/CSession.h:319-574`

**问题:** CSession 类 255 行，承担协议解析、文件传输、离线消息、登录验证等多重职责

**修复:** 分析并提出拆分方案（不强制立即执行）

- [ ] **Step 1: 分析 CSession 职责**

识别以下独立职责：
1. 协议解析（AsyncReadHead, AsyncReadBody, DecodePacket）
2. 文件传输（StartFileSend, SendNextOfflinePage, AppendFileChunk）
3. 离线消息（_offline_send_state 相关）
4. 登录验证（HandleLogin 等）
5. 发送队列（_send_queue, Send, SendBinary）

- [ ] **Step 2: 提出拆分方案**

建议拆分为：
- `ProtocolHandler` - 协议解析
- `FileTransferHandler` - 文件传输相关
- `OfflineMessageHandler` - 离线消息（已有 strand 支持）
- 保留 `CSession` - 核心 session 管理

**注意:** 此任务仅分析不强制执行，拆分需谨慎避免破坏现有架构

---

## 执行顺序

1. Task 1: 补充注释（简单，但需保持风格一致）
2. Task 2: 提取魔法数字（简单，机械替换）
3. Task 3: CSession 分析（复杂，仅分析不执行）

---

## 验证方法

每个 Task 完成后：
1. 编译项目 `cmake --build .`
2. 确认没有引入新警告