# msrChat (High Performance Distributed Instant Messaging System)

![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)
![Framework](https://img.shields.io/badge/framework-Qt%20%7C%20Boost.Asio%20%7C%20gRPC-orange.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

> **Note**: 本项目采用微服务架构设计，基于 C++17 和 Qt 开发，旨在构建一个高并发、低延迟的分布式即时通讯系统。

## 📖 项目简介 (Introduction)

**msrChat** 是一个现代化的分布式即时通讯（IM）系统。

为了解决传统单体架构在海量用户连接下的性能瓶颈，本项目采用了 **微服务架构**，将网关服务、状态服务、业务服务进行拆分。客户端通过 HTTP 协议与网关交互进行注册登录，通过 TCP 长连接与聊天服务器进行实时通信。

后端核心采用 **C++17** 标准，基于 **Boost.Asio** 异步网络库和 **gRPC** 框架，实现了高性能的网络通信和跨服务调用。前端使用 **Qt** 框架，打造了流畅且美观的用户界面。

## ✨ 核心特性 (Key Features)

*   **🏗️ 分布式微服务架构**：
    *   **GateServer (网关服务)**：基于 **Boost.Beast** 实现的 HTTP 服务器，负责用户注册、登录、负载均衡以及聊天服务器的分配。
    *   **StatusServer (状态服务)**：基于 **gRPC** 实现，维护所有 ChatServer 的在线状态和负载情况，确保连接分配的最优化。
    *   **ChatServer (聊天服务)**：(WIP) 负责维护客户端的 TCP 长连接、消息转发和即时通讯业务。

*   **⚡ 高性能网络模型**：
    *   **Boost.Asio 异步 I/O**：利用 **Epoll** (Linux) 和 **IOCP** (Windows) 技术，实现非阻塞的高并发网络处理。
    *   **I/O Context Pool**：实现了多线程 Reactor 模型，通过线程池分发 I/O 事件，充分利用多核 CPU 性能。

*   **🔄 高效通信与协议**：
    *   **gRPC & Protobuf**：服务间通信采用 gRPC，使用 Protocol Buffers 进行高效的序列化与反序列化，大幅降低网络传输开销。
    *   **自定义应用层协议**：TCP 通信采用 "Length-Field" 封包格式，完美解决 TCP 粘包/拆包问题。

*   **💾 数据存储与缓存**：
    *   **MySQL 连接池**：基于生产者-消费者模型实现的数据库连接池，支持动态扩容与空闲回收，显著提升数据库并发访问性能。
    *   **Redis 缓存**：使用 Redis 存储验证码、用户会话等临时高频数据，减轻数据库压力，提升响应速度。

*   **💻 现代化客户端 (Qt)**：
    *   **UI/UX**：使用 QSS 样式表定制界面，提供流畅的交互体验。
    *   **模块化设计**：通过单例模式 (Singleton) 管理网络模块 (HttpMgr/TcpMgr)，实现业务逻辑与网络层的解耦。

## 🛠️ 技术栈 (Tech Stack)

*   **后端开发**：
    *   **语言标准**：C++17
    *   **网络框架**：Boost.Asio, Boost.Beast
    *   **RPC 框架**：gRPC, Protobuf
    *   **数据库**：MySQL (mysql-connector-cpp), Redis (hiredis)
    *   **JSON 处理**：JsonCpp

*   **前端开发**：
    *   **框架**：Qt 5 / Qt 6 (Core, Gui, Widgets, Network)
    *   **UI 设计**：Qt Designer, QSS

*   **构建与工具**：
    *   **构建系统**：CMake
    *   **版本控制**：Git

## 🚀 快速开始 (Getting Started)

### 1. 环境准备

*   **编译器**：GCC 9+ / Clang 10+ / MSVC 2019+ (支持 C++17)
*   **构建工具**：CMake 3.15+
*   **依赖库**：
    *   Boost 1.70+
    *   gRPC & Protobuf
    *   MySQL Connector/C++
    *   hiredis (Redis Client)
    *   Qt 5.12+ 或 Qt 6.0+

### 2. 服务端配置

在 `server` 目录下各服务的 `config.ini` 中配置数据库和网络参数：

```ini
[GateServer]
Port=8080

[Mysql]
Host=127.0.0.1
Port=3306
User=root
Passwd=your_password
Name=msrchat

[Redis]
Host=127.0.0.1
Port=6379
```

### 3. 编译与运行

#### 编译服务端
```bash
cd server/GateServer
mkdir build && cd build
cmake ..
make -j4
./GateServer
```

#### 编译客户端
使用 Qt Creator 打开 `client/QmsrChat/CMakeLists.txt` 或使用命令行编译：
```bash
cd client/QmsrChat
mkdir build && cd build
cmake ..
make -j4
./QmsrChat
```

## 📄 许可证 (License)

本项目采用 [MIT License](LICENSE) 许可证。
