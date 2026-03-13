# msrChat (High Performance Distributed Instant Messaging System)

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B17-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg" alt="Platform">
  <img src="https://img.shields.io/badge/framework-Qt%20%7C%20Boost.Asio%20%7C%20gRPC-orange.svg" alt="Framework">
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License">
</p>

> **Note**: 本项目采用微服务架构设计，基于 C++17 和 Qt 开发，旨在构建一个高并发、低延迟的分布式即时通讯系统。

---

## 📖 项目简介 (Introduction)

**msrChat** 是一个现代化的分布式即时通讯（IM）系统。

为了解决传统单体架构在海量用户连接下的性能瓶颈，本项目采用了 **微服务架构**，将网关服务、状态服务、业务服务进行拆分。客户端通过 HTTP 协议与网关交互进行注册登录，通过 TCP 长连接与聊天服务器进行实时通信。

后端核心采用 **C++17** 标准，基于 **Boost.Asio** 异步网络库和 **gRPC** 框架，实现了高性能的网络通信和跨服务调用。前端使用 **Qt** 框架，打造了流畅且美观的用户界面。

### 📅 最新进展 (Latest Updates)
*   **重置密码功能**: 支持通过邮箱验证码重置用户密码，包含完整的客户端 UI 流程和后端逻辑。
*   **状态服务集成**: 登录流程现已接入 `StatusServer`，通过 gRPC 动态获取负载最低的聊天服务器，实现智能负载均衡。
*   **UI 体验优化**: 客户端新增登录、注册、重置密码页面的流畅切换，优化了错误提示和交互逻辑。

## 🏗️ 系统架构 (Architecture)

```mermaid
graph TD
    Client["Qt Client"]
    Gate["GateServer (HTTP)"]
    Status["StatusServer (gRPC)"]
    Chat["ChatServer (TCP)"]
    Redis[("Redis Cache")]
    MySQL[("MySQL DB")]

    Client -- "1. HTTP Register/Reset Pwd" --> Gate
    Client -- "2. HTTP Login" --> Gate
    Client -- "5. TCP Long Connection" --> Chat
    Gate -- "3. gRPC GetChatServer (Load Balance)" --> Status
    Gate -- "4. User Data (CRUD)" --> MySQL
    Gate -- "Verify Code/Token" --> Redis
    Status -- "Monitor Load" --> Chat
```

*   **GateServer**: HTTP 网关，负责用户注册、登录、重置密码、负载均衡分发。
*   **StatusServer**: 状态服务，维护 ChatServer 集群的健康状态和负载情况，提供登录调度。
*   **ChatServer**: TCP 聊天服务器，负责消息推送、即时通讯。
*   **Qt Client**: 跨平台客户端，集成 HTTP 和 TCP 通信模块，支持异或加密传输。

## 📂 目录结构 (Directory Structure)

```
msrChat/
├── client/                 # 客户端源码
│   └── QmsrChat/           # Qt 客户端工程
├── server/                 # 服务端源码
│   ├── GateServer/         # HTTP 网关服务器 (注册/登录/重置密码)
│   ├── ChatServer/         # TCP 聊天服务器 (消息推送)
│   └── StatusServer/       # gRPC 状态服务器 (负载均衡)
├── shared/                 # 共享代码
│   └── message.proto       # gRPC & Protobuf 定义文件
├── googletest/             # GoogleTest 单元测试框架
├── tests/                  # 项目单元测试代码
├── docs/                   # 项目文档 (调试指南、学习指南)
└── logs/                   # 运行日志
```

## ✨ 核心特性 (Key Features)

*   **⚡ 高性能网络模型**：
    *   **Boost.Asio 异步 I/O**: 基于 Epoll/IOCP 实现非阻塞 I/O，单机支持万级并发。
    *   **IO Context Pool**: 实现多线程 Reactor 模型，通过 `round-robin` 轮询分发连接，充分利用多核 CPU。

*   **🔄 高效通信与协议**：
    *   **gRPC 微服务通信**: 服务间调用采用 gRPC (Protobuf)，比 RESTful API 更高效。
    *   **自定义应用层协议**: TCP 通信采用 "Length-Field" (ID+Length+Data) 封包格式，完美解决粘包/拆包问题。

*   **💾 数据存储与优化**：
    *   **MySQL 连接池**: 基于 `std::queue` 和 `std::condition_variable` 实现的线程安全连接池，支持动态扩容与空闲回收，大幅减少连接建立开销。
    *   **Redis 缓存**: 缓存验证码、Session Token 等高频数据，减轻数据库压力。

*   **🛡️ 安全与工程化**：
    *   **密码安全**: 注册与登录密码采用 XOR 异或编码传输，增强安全性。
    *   **RAII 资源管理**: 全面使用智能指针 (`std::shared_ptr`, `std::unique_ptr`) 管理内存和资源，杜绝内存泄漏。
    *   **Singleton 单例模式**: 统一管理全局配置、网络连接池等核心组件。

## 🛠️ 技术栈 (Tech Stack)

| 类别 | 技术 | 说明 |
| :--- | :--- | :--- |
| **语言** | C++17 | 使用 lambda, smart pointers, mutex 等现代特性 |
| **网络** | Boost.Asio, Boost.Beast | 高性能异步网络库 & HTTP 库 |
| **RPC** | gRPC, Protobuf | Google 高性能 RPC 框架 |
| **数据库** | MySQL, Redis | 关系型数据库 & 内存缓存 |
| **客户端** | Qt 5 / Qt 6 | 跨平台 GUI 框架 |
| **构建** | CMake | 跨平台构建系统 |

## 🚀 编译与运行 (Build & Run)

### 1. 依赖项 (Dependencies)

*   **Compiler**: GCC 9+ / Clang 10+ / MSVC 2019+ (C++17 Support)
*   **CMake**: 3.15+
*   **Libraries**:
    *   Boost (system, thread, filesystem)
    *   gRPC & Protobuf
    *   MySQL Connector/C++
    *   hiredis (Redis Client)
    *   Qt 5.12+ (Client only)

### 2. 服务端编译 (Server)

```bash
# 编译 GateServer
cd server/GateServer
mkdir -p build && cd build
cmake .. && make -j4

# 编译 ChatServer
cd ../../ChatServer
mkdir -p build && cd build
cmake .. && make -j4

# 编译 StatusServer
cd ../../StatusServer
mkdir -p build && cd build
cmake .. && make -j4
```

### 3. 运行服务 (Run Services)

确保 MySQL 和 Redis 服务已启动，并且配置文件 (`config.ini`) 已正确配置。

```bash
# 启动 StatusServer
./server/StatusServer/build/StatusServer &

# 启动 GateServer
./server/GateServer/build/GateServer &

# 启动 ChatServer
./server/ChatServer/build/ChatServer &
```

### 4. 客户端编译 (Client)

```bash
cd client/QmsrChat
mkdir -p build && cd build
cmake .. && make -j4
./QmsrChat
```

## 📄 许可证 (License)

本项目采用 [MIT License](LICENSE) 许可证。
