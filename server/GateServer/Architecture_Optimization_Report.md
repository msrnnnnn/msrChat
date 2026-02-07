
## 7. 开发日志 (Implementation Log - Chronological)

### Step 1: 环境与基础依赖配置 (Environment Setup)
- **操作**：验证 `vcpkg` 及 `cmake` 环境，确认 `grpc`, `protobuf`, `boost`, `mysql-connector-cpp` 等库已安装。
- **目的**：确保后续微服务组件的编译与运行环境就绪。

### Step 2: StatusServer 初始构建 (StatusServer Initialization)
- **操作**：
    - 创建 StatusServer 项目结构。
    - 定义 `message.proto`，声明 `StatusService` 及 `GetChatServer` RPC 接口。
    - 生成 gRPC 代码 (`message.pb.cc`, `message.grpc.pb.cc`)。
    - 实现 `StatusServiceImpl` 类，提供基础 RPC 服务框架。
- **产出**：StatusServer 可独立编译，提供基础 gRPC 服务能力。

### Step 3: GateServer 架构优化 (GateServer Optimization)
- **操作**：
    - **Boost.Asio 重构**：将原有的同步或伪异步模型重构为基于 `AsioIOServicePool` 的全异步模型。
    - **连接池优化**：实现 `StatusGrpcClient`，管理与 StatusServer 的 gRPC 连接池，支持自动重连。
    - **配置管理**：实现 `ConfigMgr` 单例，从 `config.ini` 读取配置，消除硬编码。
- **结果**：GateServer 具备高并发连接处理能力，并能通过 gRPC 动态获取 StatusServer 信息。

### Step 4: ChatServer 核心框架搭建 (ChatServer Core Setup)
- **操作**：
    - 创建 ChatServer 项目，编写 `CMakeLists.txt`。
    - **网络层**：实现 `CServer` (监听) 和 `CSession` (会话)，基于 Boost.Asio。
    - **协议层**：设计 TLV (Type-Length-Value) 协议，实现 `MsgNode`, `SendNode`, `RecvNode` 处理粘包/半包。
    - **逻辑层**：设计 `LogicSystem` 单例，采用生产者-消费者模型，通过 `std::thread` 处理业务逻辑队列。
- **结果**：ChatServer 能够接受 TCP 连接，解析自定义协议消息。

### Step 5: 数据存储层实现 (Data Access Layer)
- **操作**：
    - **MySQL 集成**：引入 JDBC (`mysql-connector-cpp`)，实现 `MysqlPool` 连接池。
    - **DAO 模式**：封装 `MysqlDao` 和 `MysqlMgr`，提供用户数据的 CRUD 接口。
    - **Redis 集成**：(部分完成) 实现 `RedisMgr` 及连接池，用于缓存热点数据。
- **结果**：服务具备持久化存储访问能力，且通过连接池保证性能。

### Step 6: 登录逻辑完善与联调 (Login Logic & Integration)
- **操作**：
    - **Proto 扩展**：在 `message.proto` 中新增 `LoginReq`/`LoginRsp` 及 `Login` RPC。
    - **StatusServer 升级**：在 `StatusServiceImpl` 中实现 Token 生成与验证逻辑，使用 `std::map` + `std::mutex` 存储 Token。
    - **ChatServer 登录实现**：
        - 完善 `LoginHandler`，集成 `StatusGrpcClient` 进行 RPC Token 验证。
        - 集成 `MysqlMgr`，实现“内存 -> 数据库”二级用户信息查询。
    - **依赖修复**：
        - 修正 ChatServer 的 CMake 配置，解决 MySQL 链接错误和 Protobuf 头文件路径问题。
        - 统一各服务的 `const.h` 错误码定义。
- **结果**：完成了完整的登录认证链路：Client -> ChatServer -> StatusServer (Token Verify) -> ChatServer (DB Lookup) -> Client。
