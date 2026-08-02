# msrChat 系统架构与设计文档

> 版本: v2.0 | 最后更新: 2026-06-12

### 可视化架构图

> 全套系统架构 SVG 图见: [architecture-diagram.svg](./architecture-diagram.svg)

---

## 目录

1. [项目概述](#1-项目概述)
2. [总体架构](#2-总体架构)
3. [客户端架构](#3-客户端架构)
4. [服务端架构](#4-服务端架构)
5. [通信协议设计](#5-通信协议设计)
6. [核心数据流分析](#6-核心数据流分析)
7. [数据库设计](#7-数据库设计)
8. [并发与安全模型](#8-并发与安全模型)
9. [构建与部署](#9-构建与部署)

---

## 1. 项目概述

msrChat 是一个**生产级 C/S 即时通讯应用**，采用 Qt6 + QML 构建桌面客户端，Boost.Asio 构建高并发 TCP 服务端，ProtoBuf 作为序列化协议。

### 1.1 技术栈

| 层级 | 技术 | 版本 |
|------|------|------|
| 客户端 UI | Qt6 QML (Qt Quick) | 6.x |
| 客户端业务 | C++17 | - |
| 服务端框架 | Boost.Asio | 1.8x+ |
| 序列化 | Protocol Buffers | 3.x |
| 服务端数据库 | SQLite3 | 3.x |
| 日志 | spdlog | - |
| 构建系统 | CMake | 3.16+ |
| 测试 | Google Test | - |

### 1.2 目录结构

```
msrChat/
├── client/QmsrChat/        # Qt6 QML 桌面客户端
│   ├── CMakeLists.txt       # 客户端构建脚本
│   ├── config.ini           # 服务器地址配置
│   ├── resources.qrc        # Qt 资源文件
│   ├── proto/Message.proto  # Protobuf 协议定义 (共享)
│   ├── style/stylesheet.qss # 全局样式表
│   ├── src/                 # C++ 源文件 (16 个 .cpp)
│   ├── include/             # C++ 头文件 (19 个 .h)
│   ├── *.qml (18 个)        # QML UI 文件（纯 QML，无 .ui）
│   └── tests/               # Google Test 单元测试 (3 个)
├── server/ChatServer/       # Boost.Asio TCP 聊天服务器
│   ├── CMakeLists.txt       # 服务端构建脚本
│   ├── config.ini.example   # 配置示例
│   ├── include/             # 服务端头文件 (27 个 .h)
│   │   ├── Protocol/        # 协议包节点 (PacketNode.h)
│   │   └── services/        # 业务处理器头文件 (5 个)
│   ├── src/                 # 服务端源文件 (19 个 .cpp)
│   │   └── services/        # 业务处理器实现 (4 个)
│   └── tests/               # 单元测试 (6 个)
├── proto/Message.proto      # 共享 Protobuf 协议定义 (227 行)
├── docs/                    # 文档 (20 个 .md)
└── scripts/                 # 辅助脚本
```

---

## 2. 总体架构

msrChat 采用**分层 C/S 架构**，客户端与服务端通过自定义二进制 TCP 协议 + Protobuf 序列化通信。

### 2.1 架构分层

```
┌─────────────────────────────────────────────────────┐
│ Client (Qt6 + QML / C++17)                          │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐       │
│  │ QML UI    │  │ C++ Busi  │  │ TCP Net   │       │
│  │ Layer     │◄─┤ Layer     │◄─┤ Layer     │       │
│  │ (18 .qml) │  │ (10+ cls) │  │ (4 class) │       │
│  └───────────┘  └───────────┘  └───────────┘       │
└──────────────────────┬──────────────────────────────┘
                       │
              TCP Binary + Protobuf
              ┌────────────────────┐
              │  msg_id (2B)       │
              │  body_len (4B)     │
              │  body (Protobuf)   │
              └────────────────────┘
                       │
┌──────────────────────┴──────────────────────────────┐
│ Server (Boost.Asio / C++17)                         │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐       │
│  │ Network   │  │ Business  │  │ Data      │       │
│  │ Layer     │─►│ Logic     │─►│ Layer     │       │
│  │ (4 class) │  │ (5 class) │  │ (8 class) │       │
│  └───────────┘  └───────────┘  └───────────┘       │
└─────────────────────────────────────────────────────┘
```

### 2.2 设计原则

- **清晰的层次分离**: C++ 业务层与 QML UI 层通过 context property + 信号槽通信
- **异步非阻塞**: 客户端 TcpWorker 运行在独立线程，服务端全程 Boost.Asio 异步模型
- **模块化单例**: 各核心管理器以单例模式组织，生命周期通过 Init/Destroy 手动管理
- **高并发**: Boss-Worker I/O 线程池 + 分片锁 (ShardedMap) + 对象池

### 2.3 功能矩阵

| 功能 | 客户端模块 | 服务端模块 | 协议消息 |
|------|-----------|-----------|---------|
| 用户注册 | AuthController, RegisterView | LogicSystem, SQLiteMgr | ID_REGISTER_USER (1002) |
| 用户登录 | AuthController, LoginView | LogicSystem, TokenManager | ID_LOGIN_USER (1004) |
| 密码重置 | AuthController, ResetView | LogicSystem, SQLiteMgr | ID_RESET_PWD (1003) |
| 聊天登录 | AuthController, TcpMgr | LogicSystem, SessionManager | MSG_CHAT_LOGIN (1005) |
| 文本消息 | ChatController, MessageBubble | MessageRouter, SQLiteMgr | MSG_CHAT_TEXT (1006) |
| 图片传输 | ImageDownloadMgr, ImageBubble | ImageStorage | MSG_CHAT_IMAGE (1009) |
| 文件传输 | FileSendMgr, FileRecvMgr | FileTransfer | MSG_FILE_* (2001-2004) |
| 消息撤回 | ChatController, MessageActionMenu | MessageRouter, SQLiteMgr | MSG_CHAT_RECALL (1011) |
| 消息编辑 | EditMessageDialog, ChatController | MessageRouter, SQLiteMgr | MSG_CHAT_EDIT (1012) |
| 离线消息 | ChatController, ChatListModel | MessageRouter, SQLiteMgr | MSG_OFFLINE_ACK (1008) |
| 心跳保活 | TcpWorker | CSession | MSG_HELLO (1000) |

---

## 3. 客户端架构

### 3.1 入口点 (main.cpp)

启动流程：

1. 设置 Qt Quick 风格为 `Fusion`，后端为 `software`
2. 初始化数据库: `DbThreadManager::Instance().Init(db_path)`
3. 从 `config.ini` 读取服务器地址
4. 初始化 `UserMgr` 和 `TcpMgr` 单例
5. 创建 QML 引擎，注入 C++ 对象: `authController`, `chatController`, `_chatModel`
6. **双窗口架构**: 启动显示 `AuthWindow`，登录成功后销毁并创建 `ChatWindow`
7. 延迟发起 TCP 连接到服务器，进入 Qt 事件循环

### 3.2 C++ 核心类

| 类名 | 模式 | 职责 | 关键接口 |
|------|------|------|---------|
| **TcpMgr** | 单例 | TCP 连接生命周期管理，信号分发给 UI 层 | `slot_send_login_req()`, `sigLoginRsp()` |
| **TcpWorker** | QThread | 封装 QTcpSocket，心跳保活，指数退避重连 | `slotSendData()`, 30s 心跳 |
| **TcpProtocolParser** | - | 按 msg_id 反序列化分发 | `parsePacket()` |
| **RingBuffer** | - | TCP 粘包处理环形缓冲区 (最大 4MB) | `Push()`, `PopPacket()` |
| **AuthController** | QObject | QML 可调用认证控制器 | `login()`, `register_()`, `resetPassword()` |
| **ChatController** | QObject | 消息收发/撤回/编辑/文件/图片管理 | `sendMessage()`, `recallMessage()`, `sendImage()` |
| **FileCoordinator** | QObject | 文件/图片发送接收协调器 (Phase 5B.6 提取) | `sendFile()`, `recvFile()` |
| **ChatListModel** | QAbstractListModel | 消息列表数据模型 | `AddMessage()`, `MarkRecalled()`, `InsertMessageSorted()` |
| **UserMgr** | Singleton | 当前用户 UID/Token 存储 | `SetUid()`, `GetUid()`, `SetToken()` |
| **DbService** | 单例 | SQLite 消息 CRUD | `SaveMessage()`, `LoadMessageHistory()` |
| **DbWorker** | QThread | 数据库异步操作 (DbThreadManager 管理) | 信号槽调度 |
| **FileSendMgr** | 单例 | 文件发送 (64KB 分片，断点续传) | `SendFile()` |
| **FileRecvMgr** | 单例 | 文件接收 (分片写入 + MD5 校验) | `OnRecvFileChunk()` |
| **ImageDownloadMgr** | 单例 | 图片下载队列 + 缓存 + 重试 (最多 3 次) | `DownloadImage()` |
| **MessageActions** | QObject | 右键菜单操作 (回复/复制/撤回/编辑/删除) | `actionRecall()`, `actionEdit()` |
| **ProtocolStructs** | POD | 20+ 强类型消息结构体定义 | `LoginReqStruct`, `ChatTextReqStruct` 等 |

### 3.3 QML UI 层次 (18 个文件)

```
AuthWindow (独立窗口, 376px, Frameless)
├── LoginView              # 登录表单
├── RegisterView           # 两阶段注册: 表单 → 成功页
├── ResetView              # 邮箱验证重置密码
└── 认证组件:
    ├── PasswordField.qml  # 密码输入框 (含显示/隐藏切换)
    ├── CardTopAccent.qml  # 卡片顶部强调色条
    ├── AuthCardHeader.qml # 认证卡片头部 (图标+标题)
    └── AuthBanner.qml     # 认证装饰横幅

ChatWindow (独立窗口, 920x660)
└── ChatView               # 聊天主界面组合层
    ├── ChatHeader         # UID 输入栏 + 连接状态指示 + 设置菜单
    ├── ListView           # 消息列表 (model: _chatModel)
    │   └── MessageDelegate  # 按类型选择气泡
    │       ├── MessageBubble     # 文字气泡 (己方蓝/对方白)
    │       └── ImageBubble      # 图片气泡 (缩略图/加载/失败态)
    ├── ErrorBanner        # 错误提示 (底部滑入, 4s 自动消失)
    ├── ImagePreviewBar    # 发送前图片预览条
    ├── MessageInputArea   # TextArea + 发送按钮 + 图片/文件选择
    ├── EmptyState         # 空列表引导层 ("开始聊天")
    ├── FileProgressPanel  # 文件传输进度面板 (右上角浮层)
    ├── ImageViewer        # 全屏图片查看器 (缩放/旋转/翻页, Loader 延迟加载)
    ├── EditMessageDialog  # 编辑消息对话框 (Modal)
    └── MessageActionMenu  # 右键菜单 (回复/复制/撤回/编辑/删除)
```

### 3.4 UI 设计主题

| 令牌 | 值 | 用途 |
|------|-----|------|
| 主色 | `#4F46E5` (Indigo) | 按钮、标题 |
| 主色亮 | `#818CF8` | 悬停态 |
| 主色暗 | `#3730A3` | 按下态 |
| 背景 | `#F8F9FE` | 窗口背景 |
| 卡片 | `#FFFFFF` | 卡片/输入框 |
| 分割线 | `#EAE9F2` | 边框/分割线 |
| 文字主 | `#1A1A2E` | 主内容 |
| 文字次 | `#6B6A7F` | 辅助信息 |
| 成功 | `#10B981` | 成功提示 |
| 错误 | `#EF4444` | 错误提示 |

---

## 4. 服务端架构

### 4.1 入口点 (main.cpp)

启动流程：

1. 解析命令行参数 (`-d` 守护进程模式)
2. 从 `config.ini` 加载配置 (端口/数据库路径/连接池大小)
3. 初始化组件:
   - `SQLiteMgr::Init()` — 创建/连接数据库
   - `ImageStorage::Init()` — 初始化图片存储
   - `TokenManager::LoadTokensFromDB()` — 加载 Token 到内存
   - `SchemaManager` — 检查并执行 Schema 迁移
   - Debug 模式下注册 dev token (uid=1001)
4. 创建 `boost::asio::io_context` 主事件循环
5. 创建 `CServer` 在指定端口监听
6. 注册 SIGINT/SIGTERM 信号处理 (优雅关闭)
7. 启动定时任务: 过期图片清理 (每 6h) / 数据库备份 (每 6h)
8. 进入 `io_context.run()` 事件循环

### 4.2 核心类 (27 个头文件)

| 类名 | 模式 | 职责 |
|------|------|------|
| **CServer** | shared_from_this | TCP acceptor，接收连接，转发/离线消息，内含 ThreadPool |
| **CSession** | shared_from_this | 每连接会话，异步读写，协议组帧/解帧，文件/离线状态 |
| **AsioIOServicePool** | CSingleton | I/O 线程池 (Boss-Worker)，轮询分配 io_context |
| **LogicSystem** | CSingleton | 异步业务逻辑处理，ThreadPool 消费 MessageTask |
| **MessageDispatcher** | 单例 | 注册表模式: `unordered_map<msg_id, HandlerInfo>` 只读查表分发 |
| **MessageRouter** | 单例 | 在线转发/离线存储，全局递增 msg_id |
| **SessionManager** | 单例 | uid→session + uuid→session 双向映射 (32 分片) |
| **TokenManager** | 单例 | uid→token 内存缓存 (16 分片)，7 天有效期，DB 持久化 |
| **SQLiteMgr** | 单例 | SQLite 连接池 (8 连接) + Repository 门面 |
| **AuthRepository** | 注入 | 认证数据仓库 (用户注册/登录/验证码/Token) |
| **MessageRepository** | 注入 | 消息数据仓库 (CRUD + 离线 + 撤回/编辑通知队列) |
| **ImageStorage** | 单例 | 图片 BLOB 分片存储，7 天过期 |
| **FileTransfer** | 单例 | 文件传输任务路由 |
| **SchemaManager** | - | Schema 版本管理 + 迁移 |
| **RateLimiter** | 单例 | Per-user 令牌桶限流 (默认 10/s, burst 20) |
| **NonceCache** | - | 防重放 nonce 缓存 |
| **ThreadPool** | - | 生产者-消费者线程池 (最大队列 10,000) |
| **ShardedMap** | 模板 | 32 分片哈希表，降低锁竞争 |
| **ObjectPool** | 模板 | RecvNode/SendNode 对象复用 |

### 4.3 并发模型 (Boss-Worker)

```
Main Thread (io_context)
  └── CServer (acceptor)
       └── accept → 新连接 → 分配到 AsioIOServicePool

AsioIOServicePool (N × io_context + N 线程)
  └── 每个 io_context 运行多个 CSession
       └── CSession::AsyncReadHead → AsyncReadBody → Dispatch

LogicSystem (独立 ThreadPool)
  └── 异步消费 MessageTask (反序列化、业务处理、DB 操作)

SQLiteConnectionPool (8 连接)
  └── Acquire/Release RAII 管理

CSession::strand — 保证发送队列串行化
```

### 4.4 关键设计决策

- **CSession 使用 strand**: 确保异步操作的线程安全性
- **MessageDispatcher 注册表模式**: 启动时注册所有 handler，运行时只读查表，无锁
- **ShardedMap 分片锁**: 32 个分片降低 SessionManager 和 TokenManager 锁竞争
- **ObjectPool 对象池**: 复用 RecvNode/SendNode/FileTransferTask，减少动态分配
- **读超时检测**: 30s，CAS 防止并发读
- **CAS 原子标志**: 防止 Session 的并发读/写/登录

---

## 5. 通信协议设计

### 5.1 二进制协议格式

```
| 消息 ID (2 bytes) | 消息体长度 (4 bytes) | 消息体 (Protobuf) |
|      uint16        |        uint32        |  变长 (max 1MB)   |
```

- 头部固定 6 字节，小端序
- 消息体使用 Protobuf 序列化
- 单包最大 1MB

### 5.2 消息类型定义

| ID | 名称 | 方向 | 说明 |
|----|------|------|------|
| 1000 | MSG_HELLO | 双向 | 心跳连接问候 |
| 1001 | ID_GET_VARIFY_CODE | C→S | 获取验证码 |
| 1002 | ID_REGISTER_USER | C→S | 用户注册 |
| 1003 | ID_RESET_PWD | C→S | 重置密码 |
| 1004 | ID_LOGIN_USER | C→S | 用户登录 |
| 1005 | MSG_CHAT_LOGIN | C→S | 聊天登录 (Token 鉴权) |
| 1006 | MSG_CHAT_TEXT | C↔S | 聊天文本消息 |
| 1007 | MSG_CHAT_ACK | S→C | 消息送达确认 |
| 1008 | MSG_OFFLINE_ACK | C→S | 离线消息分页确认 |
| 1009 | MSG_CHAT_IMAGE | C↔S | 图片消息 |
| 1010 | MSG_IMAGE_DOWNLOAD_RSP | S→C | 图片下载响应 |
| 1011 | MSG_CHAT_RECALL | C→S | 消息撤回请求 |
| 1012 | MSG_CHAT_EDIT | C→S | 消息编辑请求 |
| 1013 | MSG_IMAGE_DOWNLOAD_REQ | C→S | 图片下载请求 |
| 1014 | MSG_CHAT_RECALL_NOTIFY | S→C | 撤回通知广播 |
| 1015 | MSG_CHAT_EDIT_NOTIFY | S→C | 编辑通知广播 |
| 2001 | MSG_FILE_TRANSFER_REQ | C→S | 文件传输请求 |
| 2002 | MSG_FILE_TRANSFER_RSP | S→C | 文件传输响应 |
| 2003 | MSG_FILE_TRANSFER_DATA | C→S | 文件分片数据 |
| 2004 | MSG_FILE_TRANSFER_ACK | S→C | 文件传输确认 |

### 5.3 TCP 连接管理 (客户端 TcpWorker)

- 运行在独立 `QThread` 中
- **心跳保活**: 30s 间隔发送 `MSG_HELLO`，检测 pong 超时
- **指数退避重连**: 断开后 1s → 2s → 4s → ... → 30s，最大间隔 30s
- **粘包处理**: `RingBuffer` 环形缓冲区，逐包解析 6B 头部 + 变长消息体
- **连接状态机**: Idle → Connecting → Connected → Reconnecting → Stopping

---

## 6. 核心数据流分析

### 6.1 登录流程

```
LoginView.qml
  └─ authController.login(user, pass)
       └─ AuthController::login()
            └─ TcpMgr::slot_send_login_req()
                 └─ TcpWorker::slotSendData()
                      └─ QTcpSocket::write() ── TCP ──► CSession::AsyncReadHead()
                                                           └─ MessageDispatcher::Dispatch()
                                                                └─ Login handler
                                                                     ├─ SQLiteMgr::LoginUser()
                                                                     └─ TokenManager::SetToken()
                                                                     └─ CSession::Send() ← LoginRsp
       ◄── TcpProtocolParser::parseLoginPacket()
       ◄── TcpMgr::sigLoginRsp()
       ◄── AuthController::slotLoginRsp()
            ├─ AuthController::login() → send chat login
            │    └─ TcpMgr::slot_send_chat_login_req()
            │         └─ ... TCP ... ──► Server validates token
            │    ◄── TcpMgr::sigChatLoginRsp()
            │    ◄── AuthController::slotChatLoginRsp()
            │         ├─ UserMgr::SetUid/SetToken
            │         └─ emit chatLoginSuccess()
            │              └─ main.cpp: destroy AuthWindow → create ChatWindow
            └─ AuthController::chatLoginReady = true
```

### 6.2 聊天消息发送流程

```
ChatView.qml (用户点击发送)
  └─ chatController.sendMessage(content)
       └─ ChatController::sendMessage()
            ├─ 生成 client_msg_id (QUuid)
            ├─ 通过 TcpMgr 发送 ChatTextReqStruct
            │    └─ TcpWorker::slotSendData
            │         └─ Protobuf 序列化 + 二进制组帧
            │         └─ QTcpSocket::write() ── TCP ──► Server
            ├─ _pending_messages 插入待确认记录
            └─ _chat_model->AddMessage() → UI 显示 "发送中"

CServer: CSession::AsyncReadBody() → 完整消息体
  └─ LogicSystem::PostTask(MessageTask)
       └─ MessageDispatcher::Dispatch(session, msg_id, body_data)
            ├─ 鉴权检查 (requires_auth && session.GetUserUid() > 0)
            └─ ChatService::HandleChatText()
                 ├─ 反序列化 Protobuf
                 ├─ 生成 server_msg_id (全局递增)
                 ├─ MessageRepository::SaveMessage() (持久化)
                 ├─ MessageRouter::ForwardMessage(to_uid, msg_data)
                 │    ├─ SessionManager::GetSession(to_uid)
                 │    ├─ [在线] → CSession::Send() 直接转发
                 │    └─ [离线] → MessageRepository::SaveOfflineMessage()
                 ├─ CSession::Send() → ChatAck 回发发送方
                 └─ 发送离线消息 (如目标刚上线)
```

### 6.3 消息撤回/编辑流程

```
ContextMenu → recall/edit
  └─ ChatController::recallMessage(uid, msg_id)
       └─ TcpMgr → MSG_CHAT_RECALL (1011)
            └─ Server:
                 ├─ 验证 2 分钟时间窗口
                 ├─ 验证消息所有权
                 └─ SQLiteMgr::MarkRecalled()
                 └─ MSG_CHAT_RECALL_NOTIFY → 对方客户端
                      └─ ChatController::OnRecvRecallNotify()
                           └─ ChatListModel::MarkRecalled()
```

### 6.4 离线消息恢复

```
ChatController::initialize() (ChatWindow 创建时调用)
  └─ ChatController 监听 TcpMgr 信号

服务端在用户聊天登录成功后:
  └─ CServer::SendOfflineMessages(uid, session)
       ├─ 分页发送 (每页 50 条)
       ├─ 客户端接收后发送 MSG_OFFLINE_ACK 确认页码
       └─ 服务端继续发送下一页直到全部完毕
       └─ FlushRecallNotifies() 发送待处理撤回通知

客户端:
  └─ ChatController::slotOnChatTextMsg()
       └─ ChatListModel::InsertMessageSorted() 排序插入
```

### 6.6 文件传输流程 (新增)

```
发送方:
  └─ FileCoordinator::sendFile(file_path)
       └─ FileSendMgr::SendFile()
            ├─ 分片读取 (64KB/chunk)
            ├─ MSG_FILE_REQ (2001) → 元数据 + 总大小
            ├─ 服务端 FileTransfer → MSG_FILE_RSP (2002) → 确认参数
            ├─ MSG_FILE_CHUNK (2003) → 分片数据流
            │    └─ 每片发送后等待 MSG_FILE_ACK (2004) → 继续下一片
            └─ 断点续传: 重连后从上次 offset 继续

接收方:
  └─ FileRecvMgr::OnRecvFileChunk()
       ├─ 分片写入临时文件
       ├─ 全部接收后 MD5 完整性校验
       └─ 校验通过 → 移动到目标路径
```

### 6.5 图片传输流程

```
发送方:
  ├─ ChatController::sendImage(image_path)
  ├─ 读取图片文件 → 编码
  ├─ TcpMgr → MSG_CHAT_IMAGE (1009): 元数据 + 文件分片
  └─ UI 显示 ImageBubble (缩略图预览)

服务端 ImageStorage:
  ├─ 接收图片分片 → BLOB 存储到 SQLite
  ├─ 返回 image_id
  └─ 7 天自动过期

接收方:
  ├─ ImageDownloadMgr::DownloadImage(image_id)
  ├─ 本地缓存: 检查磁盘 → 检查内存 → 下载
  ├─ TcpMgr → MSG_IMAGE_DOWNLOAD_REQ (1013)
  ├─ 收到 MSG_IMAGE_DOWNLOAD_RSP (1010)
  ├─ 写入本地缓存文件
  └─ ImageBubble 显示 (缩略图/加载态/失败态)
```

---

## 7. 数据库设计

### 7.1 客户端本地数据库 (chat_messages.db)

由 `DbService` 管理，存储聊天消息。

**messages 表结构:**

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER PK | 自增主键 |
| client_msg_id | TEXT | 客户端生成的消息 ID (QUuid) |
| server_msg_id | INTEGER | 服务端分配的消息 ID |
| from_uid | INTEGER | 发送方 UID |
| to_uid | INTEGER | 接收方 UID |
| content | TEXT | 消息文本内容 |
| timestamp | INTEGER | 消息时间戳 |
| status | INTEGER | 状态: 0=发送中, 1=已发送, 2=失败 |
| type | INTEGER | 类型: 0=文本, 1=图片, 2=文件 |
| image_id | TEXT | 图片 ID |
| image_path | TEXT | 本地图片缓存路径 |
| image_width | INTEGER | 图片宽度 |
| image_height | INTEGER | 图片高度 |
| image_ext | TEXT | 图片扩展名 |
| edited | INTEGER | 是否已编辑 (0/1) |
| edited_at | INTEGER | 编辑时间戳 |
| recalled | INTEGER | 是否已撤回 (0/1) |
| recalled_at | INTEGER | 撤回时间戳 |

消息查询按 `timestamp` 排序，并按 `recalled` 过滤。

### 7.2 服务端数据库

由 `SQLiteMgr` 管理，使用连接池（8 连接）。

**核心表（推断设计）:**
- `users`: uid, username, password_hash, email, created_at
- `messages`: msg_id, from_uid, to_uid, content, timestamp, recalled, edited
- `offline_messages`: uid, msg_data, page_seq
- `tokens`: uid, token, expires_at
- `verify_codes`: email, code, expires_at
- `images`: image_id, uid, data (BLOB), created_at

**连接池设计:**
- `SQLiteConnectionPool` 管理 8 个连接
- `Acquire()` / `Release()` RAII 自动管理
- 连接通过 `std::unique_ptr` + 自定义 Deleter 自动归还

---

## 8. 并发与安全模型

### 8.1 客户端并发

| 线程 | 职责 |
|------|------|
| Main Thread (Qt 事件循环) | QML UI 渲染，信号槽调度 |
| TcpWorker QThread | QTcpSocket 异步读写，心跳，重连 |
| DbWorker QThread | SQLite 数据库异步写入 |
| FileSend/Recv 异步任务 | 文件分片读写 |

跨线程通信: 全部通过 Qt 信号槽机制

### 8.2 服务端并发

| 组件 | 线程模型 |
|------|---------|
| CServer Acceptor | Main io_context 单线程 |
| AsioIOServicePool | N 个 io_context，各绑 1 线程 |
| LogicSystem ThreadPool | M 个 worker 线程消费 MessageTask |
| SQLiteMgr 连接池 | 8 个连接，每个线程独立使用 |
| ShardedMap | 32 分片，每分片独立 mutex |

**安全机制:**
- CSession 使用 `boost::asio::strand` 串行化异步操作
- CAS 原子标志防止并发读/写/登录
- 读超时检测 (30s)
- 密码 SHA-256 加盐哈希
- Token 鉴权 (服务端 MessageDispatcher 按消息类型配置鉴权)

### 8.3 安全措施

| 措施 | 实现 |
|------|------|
| 密码存储 | SHA-256 + 随机盐值 (`Utils::hashPassword`) |
| 会话鉴权 | 登录后生成 Token，后续操作需 Token 校验 |
| Token 持久化 | 内存缓存 + 数据库持久化，服务端重启不丢失 |
| 消息防篡改 | 消息撤回/编辑需验证所有权和时间窗口 |
| 连接安全 | 读超时 + CAS 防并发 + strand 串行化 |

---

## 9. 构建与部署

### 9.1 客户端构建

```bash
# 先决条件: Qt6, CMake 3.16+, Protobuf
cd client/QmsrChat
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build

# 运行
./build/QmsrChat
```

CMake 配置要点:
- C++17, Qt6 (Network, Quick, Qml, Sql, QuickControls2, QuickLayouts)
- FetchContent 自动下载 Google Test
- 自动部署 config.ini 到构建输出目录

### 9.2 服务端构建

```bash
# 先决条件: Boost, CMake, SQLite3, Protobuf, spdlog, OpenSSL
cd server/ChatServer
cmake -B build
cmake --build build

# 运行 (默认端口)
./build/ChatServer
# 守护进程模式
./build/ChatServer -d
```

### 9.3 跨平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Windows | MSVC / MinGW | 已验证 |
| Linux | GCC / Clang | 已验证 |
| macOS | Clang | 已验证 |

---

## 附录: 核心模块依赖关系

```
main.cpp
├── TcpMgr (单例)
│   ├── TcpWorker (QThread)
│   │   ├── QTcpSocket
│   │   └── RingBuffer
│   └── TcpProtocolParser
├── AuthController (QML 可调用)
│   ├── TcpMgr
│   └── UserMgr (Singleton)
├── ChatController (QML 可调用)
│   ├── TcpMgr
│   ├── ChatListModel (QAbstractListModel)
│   ├── UserMgr
│   ├── FileCoordinator
│   │   ├── FileSendMgr → TcpMgr
│   │   └── FileRecvMgr → TcpMgr
│   └── MessageActions
│       └── TcpMgr (撤回/编辑通过信号发送)
├── DbService (单例)
│   ├── DbWorker (QThread)
│   └── SQLite3
└── ImageDownloadMgr (单例)
    └── TcpMgr
```

---

*本文档基于 msrChat 项目代码分析生成，版本 v2.0，2026-06-12 更新。*
