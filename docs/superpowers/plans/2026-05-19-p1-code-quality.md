# P1 代码质量问题修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 P1 级别的代码质量问题（循环依赖、代码冗余、命名问题）

**Architecture:** 分析 3 个 P1 问题，按优先级分类修复。循环依赖通过前向声明解决，代码冗余通过提取公共方法解决。

**Tech Stack:** C++, Qt, boost::asio

---

## 问题列表

| # | 问题 | 文件 | 严重度 |
|---|------|------|--------|
| 1 | CSession/CServer 循环依赖 | CSession.h:7 ↔ CServer.h:7 | P1 |
| 2 | CSession 错误处理重复 4+ 处 | CSession.cpp:226-683 | P1 |
| 3 | DbThreadPool 名不副实 | DbWorker.h:41 | P1 |

---

## Task 1: CSession/CServer 循环依赖

**Files:**
- Modify: `server/ChatServer/include/CSession.h`
- Modify: `server/ChatServer/include/CServer.h`

**问题:** CSession.h 直接 include CServer.h，形成双向依赖

**修复:** 在 CSession.h 中使用前向声明 `class CServer;`，将 include 移到 .cpp 文件

- [ ] **Step 1: 在 CSession.h 中将 `#include "CServer.h"` 替换为前向声明**

```cpp
// 移除 #include "CServer.h"
// 添加 class CServer;

// 如果有使用 CServer* 或 CServer& 的地方，保留不变
```

- [ ] **Step 2: 在 CSession.cpp 中添加 `#include "CServer.h"`**

---

## Task 2: CSession 错误处理重复

**Files:**
- Modify: `server/ChatServer/src/CSession.cpp`

**问题:** 4+ 处错误处理代码几乎相同（226-241, 314-331, 481-498, 666-683）

**修复:** 提取公共逻辑为私有方法 `CleanupSession()`

- [ ] **Step 1: 添加私有方法声明**

```cpp
// CSession.h 中 private 部分添加
private:
    void CleanupSession(const std::string &error_msg = "");
```

- [ ] **Step 2: 实现 CleanupSession()**

```cpp
void CSession::CleanupSession(const std::string &error_msg)
{
    if (!error_msg.empty()) {
        spdlog::error("[CSession] {}: {}", uuid(), error_msg);
    }
    // 通用清理逻辑：停止所有计时器、关闭 socket、移除 session 等
    // 参考现有代码中的重复逻辑
}
```

- [ ] **Step 3: 替换 4 处重复错误处理调用**

将以下位置调用 CleanupSession():
- AsyncReadHead 错误处理 (226-241)
- AsyncReadBody 错误处理 (314-331)
- AsyncWriteMsg 错误处理 (481-498)
- AsyncReadBinBody 错误处理 (666-683)

---

## Task 3: DbThreadPool 命名问题

**Files:**
- Modify: `client/QmsrChat/include/DbWorker.h:41`

**问题:** `DbThreadPool` 名不副实，实际是单线程任务投递器（1个QThread + 1个DbWorker），不是线程池

**修复:** 重命名为 `DbWorker` 或 `DbTaskQueue`

- [ ] **Step 1: 将类名 `DbThreadPool` 改为 `DbWorker` 或 `DbTaskQueue`**

如果保持向后兼容，可以添加 typedef：
```cpp
using DbThreadPool = DbWorker;  // 兼容旧代码
```

---

## 执行顺序

1. Task 1: CSession/CServer 循环依赖（基础，修复后利于后续开发）
2. Task 2: CSession 错误处理重复（代码质量）
3. Task 3: DbThreadPool 命名（可选，影响小）

---

## 验证方法

每个 Task 完成后：
1. 编译项目 `cmake --build .`
2. 运行客户端和服务端
3. 测试基本功能：登录、发送消息、接收消息

**整体验证:**
- 检查循环依赖是否消除（CServer.h 不再被 CSession.h 直接依赖）
- 确认 DbThreadPool 命名符合实际功能