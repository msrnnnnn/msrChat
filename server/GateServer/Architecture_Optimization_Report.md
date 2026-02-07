# 开发日志 - GateServer 架构修复与登录功能实现

## 1. 核心架构修复 (Architecture Fixes)

针对 GateServer 存在的资源管理风险和并发安全隐患，进行了系统性的架构修复与优化。

### 1.1 MySQL 连接池 RAII 重构 (MysqlPool)
- **移除手动归还**：移除了原有的 `std::unique_ptr` + 手动 `returnConnection` 模式，该模式在异常发生时极易导致连接泄漏。
- **引入智能指针**：`getConnection` 改为返回 `std::shared_ptr<sql::Connection>`，并绑定自定义删除器 (Deleter)。
- **生命周期安全**：当连接对象离开作用域（析构）时，自动触发回调将连接归还给连接池，实现**零泄漏**。
- **自动重连**：在获取连接时增加了 `isValid()` 检查，若连接已断开则自动尝试重连，增强了抗网络抖动能力。

**代码变更**:
- `include/MysqlPool.h`: 修改 `getConnection` 签名，实现自定义 Deleter。
- `src/MysqlDao.cpp`: 移除所有显式的 `pool_->returnConnection` 调用。

### 1.2 HTTP 协议健壮性增强 (HttpConnection)
- **挂起修复 (Hang Fix)**：原逻辑仅处理 GET/POST，导致 HEAD/PUT 等请求使服务器无响应并挂起连接。现增加默认分支，统一返回 `405 Method Not Allowed`。
- **安全解码 (Safe Decode)**：重构 `UrlDecode` 函数，移除 Release 模式下失效的 `assert`，替换为严格的运行时边界检查，防止恶意构造的 URL 导致内存越界崩溃。

**代码变更**:
- `src/HttpConnection.cpp`: 增加 405 响应逻辑；重写 `UrlDecode` 安全检查。

### 1.3 IO 线程池启动优化 (AsioIOServicePool)
- **启动顺序竞争修复**：原构造函数在循环中同时进行资源创建和线程启动。现改为两阶段初始化：先完成所有 `io_context` 和 `work_guard` 的创建，再统一启动线程，防止线程在资源未就绪时抢跑。

**代码变更**:
- `src/AsioIOServicePool.cpp`: 分离资源初始化循环与线程启动循环。

## 2. 用户登录功能实现 (User Login)

### 2.1 业务逻辑实现 (LogicSystem)
- **路由注册**：在构造函数中注册 `/user_login` POST 路由。
- **流程实现**：
  1.  **JSON 解析**：解析请求体中的用户名和密码。
  2.  **数据库验证**：调用 `MysqlMgr` 校验密码正确性。
  3.  **RPC 调度**：调用 `StatusGrpcClient` 向 StatusServer 请求分配 ChatServer。
  4.  **响应封装**：返回 Token、Host、Port 及用户信息。
- **错误处理**：完善了 JSON 错误、密码错误 (1009)、RPC 失败 (1010) 等错误码的返回。

### 2.2 数据库层扩展 (MySQL Layer)
- **接口开放**：将 `MysqlMgr` 中的 `CheckEmail` 和 `UpdatePwd` 从 `private` 调整为 `public`，以支持业务层调用（解决了编译错误）。
- **密码验证**：
  - `MysqlMgr`: 新增 `CheckPwd` 转发接口。
  - `MysqlDao`: 实现 `CheckPwd` SQL 查询逻辑，通过 `SELECT * FROM user WHERE name=?` 验证密码。

### 2.3 RPC 客户端实现 (StatusGrpcClient)
- **新服务接入**：封装 `StatusGrpcClient` 单例类，负责与 StatusServer 通信。
- **连接池实现**：
  - 实现 `StatusConPool` 管理 gRPC Stub 连接。
  - **复用 RAII 模式 (优化点)**：教程中使用 `Defer` 类手动归还连接，本项目中优化为 `std::shared_ptr` + 自定义 Deleter 机制。
    - **优势**：消除了手动调用 `returnConnection` 的负担，防止因遗忘或异常导致连接泄漏。
- **配置集成**：自动读取 `config.ini` 中的 `[StatusServer]` 配置，并适配项目现有的 `ConfigMgr::GetInstance()` 接口。

## 3. 协议与接口定义

- **Proto 定义**: 
  - 更新 `message.proto`，新增 `StatusService` RPC 服务定义。
  - 新增 `GetChatServerReq` (uid) 和 `GetChatServerRsp` (host, port, token)。
- **错误码**: 在 `const.h` 中新增 `PasswdInvalid` (1009) 和 `RPCGetFailed` (1010)。

## 4. StatusServer 服务实现 (New Service)

为了支持聊天服务器的负载均衡和状态管理，从零构建了 StatusServer 服务。

### 4.1 服务架构
- **独立进程**：StatusServer 作为一个独立的 gRPC 服务运行，监听 `50052` 端口。
- **组件复用**：直接复用 GateServer 中成熟的组件 (`ConfigMgr`, `MysqlMgr`, `RedisMgr`, `AsioIOServicePool`)，保证了代码的一致性和稳定性。
- **构建系统**：创建了独立的 `CMakeLists.txt`，支持 protobuf 和 grpc 的自动编译与链接。

### 4.2 核心逻辑 (StatusServiceImpl)
- **接口实现**：实现了 `GetChatServer` RPC 接口。
- **负载均衡**：
  - 维护一个 ChatServer 列表（读取自 `config.ini`）。
  - **线程安全轮询**：使用 `std::atomic<int>` 维护轮询索引 `_server_index`，替代了教程中非线程安全的 `int` 自增，确保在高并发下分发逻辑的正确性。
- **Token 生成**：使用 `boost::uuids` 生成唯一 Token，用于后续的连接验证。

### 4.3 配置管理
- **config.ini**：新增 `[StatusServer]`, `[ChatServer1]`, `[ChatServer2]` 配置段，支持灵活的服务地址配置。

## 5. ChatServer 服务实现 (New Service)

为了支持客户端的即时通讯功能，从零构建了 ChatServer 服务。

### 5.1 服务架构
- **TCP服务**：ChatServer 是一个高性能的 TCP 服务器，基于 Boost.Asio 实现。
- **组件复用**：复用 `AsioIOServicePool` 进行多线程 IO 处理，复用 `ConfigMgr` 和 `Singleton`。
- **会话管理**：
  - `CServer`：负责监听端口，接受新连接，并管理所有活跃的 `CSession`。
  - `CSession`：代表一个客户端连接，负责数据的收发。使用 `std::enable_shared_from_this` 保证异步操作期间对象的生命周期安全。

### 5.2 核心逻辑
- **异步接收**：`CServer::StartAccept` 使用异步 Accept 模式，将新连接分发给 `AsioIOServicePool` 中的 IO 线程。
- **读写处理**：`CSession` 实现了异步读取逻辑，并在读取错误或连接关闭时自动清理会话。
- **UUID标识**：每个 Session 在创建时生成唯一的 UUID，用于在 Server 中进行索引和管理。

### 5.3 业务逻辑层实现 (LogicSystem & Session Layer)
- **协议解析 (Session Layer)**：
  - **TLV格式**：实现了 Tag-Length-Value 协议头解析（2字节ID + 2字节长度），解决 TCP 粘包/半包问题。
  - **MsgNode/RecvNode**：设计了专门的消息节点类管理收发缓冲区，支持动态内存管理。
  - **异步读写**：
    - `AsyncReadHead` -> `AsyncReadBody` 状态机模式循环读取数据。
    - 发送队列 `_send_que` 配合 `_send_lock` 保证多线程发送安全性，实现发送合并优化。
- **逻辑分发 (LogicSystem)**：
  - **生产者-消费者模型**：使用 `std::queue` + `std::condition_variable` 实现高效的消息解耦。
    - Session 层作为生产者将解析好的 `LogicNode` 投递到队列。
    - 逻辑线程作为消费者阻塞等待并处理消息。
  - **回调机制**：基于 `std::map<short, FunCallBack>` 实现消息 ID 到处理函数的映射，支持灵活扩展业务逻辑。
  - **JSON 交互**：集成了 `jsoncpp`，在 `LoginHandler` 中实现了基于 JSON 的登录请求解析与回显。

---

## 6. 文件变更列表

| 文件 | 类型 | 描述 |
| :--- | :--- | :--- |
| `include/MysqlPool.h` | 修改 | 重构为 RAII 自动归还模式，增加自动重连。 |
| `src/MysqlDao.cpp` | 修改 | 实现 `CheckPwd`；移除所有手动 `returnConnection`。 |
| `include/MysqlMgr.h` | 修改 | 新增 `CheckPwd`；公开 `CheckEmail/UpdatePwd` 权限。 |
| `src/HttpConnection.cpp` | 修改 | 修复非 GET/POST 挂起问题；修复 URL 解码崩溃风险。 |
| `src/AsioIOServicePool.cpp` | 修改 | 优化线程启动顺序，防止竞争。 |
| `src/LogicSystem.cpp` | 修改 | 注册 `/user_login` 路由，实现登录业务逻辑。 |
| `include/StatusGrpcClient.h` | **新增** | StatusServer gRPC 客户端及连接池声明。 |
| `src/StatusGrpcClient.cpp` | **新增** | gRPC 客户端实现（含 RAII 连接管理）。 |
| `message.proto` | 修改 | 新增 `StatusService` 定义。 |
| `include/const.h` | 修改 | 新增登录相关错误码。 |
| `CMakeLists.txt` | 修改 | 添加 `StatusGrpcClient.cpp` 到编译列表。 |
| `server/StatusServer/` | **新增** | 新增 StatusServer 服务项目目录及其所有源码。 |
| `server/ChatServer/` | **新增** | 新增 ChatServer 服务项目目录及其所有源码。 |

## 7. 错误修复 (Bug Fixes)

- **编译错误修复**:
  - 修正了 `LogicSystem.cpp` 调用 `MysqlMgr::CheckEmail` 时提示 `is private` 的错误。
  - 修正了 `StatusGrpcClient` 中遗留的手动 `returnConnection` 代码，统一为 RAII 模式。
- **运行时修复**:
  - 解决了 GateServer 启动时端口被占用的问题（清理了旧进程）。
  - 修复了 `config.ini` 读取逻辑，确保能正确连接 MySQL 和 StatusServer。
