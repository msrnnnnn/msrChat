# msrChat 项目上下文（2026-04-13）

## 项目概述

msrChat 是一个即时通讯系统，正在进行从**微服务架构**到**单服务器架构**的重构。

## 当前架构（重构后）

```
Qt Client ←→ ChatServer (TCP + TLV) ←→ SQLite
```

- **ChatServer**：单一 TCP 服务器，内置用户认证（注册/登录/重置密码）、消息收发、文件传输
- **Qt Client**：通过 TcpMgr 走单一 TCP 长连接通信
- **数据库**：SQLite3 本地存储（用户、验证码、聊天记录）

## 重构的核心变更

### 旧架构（已废弃）
- GateServer（HTTP）：认证
- StatusServer（gRPC）：负载均衡
- MySQL + Redis：数据存储
- 客户端用 HTTP 做认证、TCP 做聊天

### 新架构（当前）
- ChatServer 统一处理认证 + 聊天，全部走 TCP
- SQLite 替代 MySQL + Redis
- 客户端统一用 TcpMgr（删除 httpmanagement）

## 已完成的重构内容

### 服务端（server/ChatServer/）
| 文件 | 作用 |
|---|---|
| `CSession.cpp` | 5个TCP handler：Login、Register、LoginAuth、GetVerifyCode、ResetPwd |
| `CServer.h/cpp` | SetToken/CheckToken/RemoveToken token管理 |
| `SQLiteMgr.h/cpp` | 用户注册/登录/验证码/重置密码/消息存储 |
| `Protocol/TLVProtocol.h/cpp` | TLV (Type-Length-Value) 协议封包 |
| `Protocol/BaseProtocol.h/cpp` | 基础协议节点 |
| `ThreadPool.h/cpp` | 任务线程池 |
| `MsgQueue.h` | 异步消息队列 |
| `FileTransfer.h/cpp` | 文件传输（内存映射+sendfile） |
| `ObjectPool.h` | 对象池 |
| `ShardedMap.h` | 分片哈希表 |
| `const.h` | 协议常量（包长度等） |
| `CMakeLists.txt` | 已移除 httpmanagement 依赖 |

### 客户端（client/QmsrChat/）
| 文件 | 作用 |
|---|---|
| `logindialog.cpp/h` | 从 HTTP 改为 TcpMgr 信号槽 |
| `registerdialog.cpp/h` | 从 HTTP 改为 TcpMgr 信号槽 |
| `resetdialog.cpp/h` | 从 HTTP 改为 TcpMgr 信号槽 |
| `tcpmgr.cpp/h` | TCP 通信管理（已有，未改） |
| `include/ChatModel.h` | 聊天数据模型 |
| `include/DbMgr.h` | 本地数据库管理 |
| `src/ChatModel.cpp` | 聊天数据模型实现 |
| `src/DbMgr.cpp` | 本地数据库实现 |
| `CMakeLists.txt` | 已移除 httpmanagement |
| `httpmanagement.cpp/h` | **已删除** |

### 已删除
- `server/GateServer/` 全部
- `server/StatusServer/` 全部
- `shared/` 全部
- `googletest/` 全部
- `tests/` 全部
- `docs/` 全部
- `client/httpmanagement.cpp/h`
- `git` 空文件

## 待完成

1. **ChatDialog 对接**：chatdialog.cpp/h 可能还残留旧的 HTTP 或业务逻辑，需要检查并确保走 TcpMgr
2. **Linux 编译验证**：服务端 `cd server/ChatServer && mkdir build && cd build && cmake .. && make -j4`
3. **全流程联调**：注册→登录→聊天

## Git 状态
- 当前分支：`new`
- 最新 commit：`be4916c` "docs: 更新README为新架构（单服务器+SQLite+TLV协议）"
- `origin/new` 已推送

## 关键协议 ID（const.h）
```cpp
const int MSG_LOGIN = 1001;
const int MSG_LOGIN_RSP = 1002;
const int MSG_REGISTER = 1003;
const int MSG_REGISTER_RSP = 1004;
const int MSG_LOGIN_AUTH = 1005;
const int MSG_LOGIN_AUTH_RSP = 1006;
const int MSG_GET_VERIFY_CODE = 1007;
const int MSG_GET_VERIFY_CODE_RSP = 1008;
const int MSG_RESET_PWD = 1009;
const int MSG_RESET_PWD_RSP = 1010;
const int MSG_CHAT = 2001;
const int MSG_CHAT_RSP = 2002;
const int MSG_FILE = 3001;
const int MSG_FILE_RSP = 3002;
```

## TcpMgr 信号类型（客户端）
```cpp
enum class ResponeType { Login, Register, ResetPwd, GetVerifyCode, LoginAuth };
```

## 建议的新窗口 prompt
```
这是一个 C++ 即时通讯项目（msrChat），正在进行微服务到单服务器的架构重构。
服务端：ChatServer（Boost.Asio TCP + SQLite + TLV协议）
客户端：Qt + TcpMgr（统一TCP连接）
请先阅读 server/ChatServer/src/CSession.cpp 了解 TCP handler，
阅读 client/QmsrChat/tcpmgr.h 了解客户端通信层，
然后检查 chatdialog.cpp 是否需要重构为 TcpMgr。
```
