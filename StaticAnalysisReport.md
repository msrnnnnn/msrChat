# 静态代码分析报告

**日期:** 2026-02-05
**项目:** msrchat
**范围:** 客户端 (QmsrChat) 和 服务端 (GateServer)

## 执行摘要
本项目对源代码进行了手动静态分析。发现了一些问题，范围从关键的稳定性风险到潜在的安全漏洞。

## 发现汇总

| ID | 严重程度 | 文件 | 组件 | 问题类型 | 描述 |
|----|----------|------|-----------|------------|-------------|
| 1 | **高** | MysqlPool.h | 服务端 | 死锁 | 如果初始化失败，连接池将无限期等待。 |
| 2 | **高** | HttpConnection.cpp | 服务端 | 安全 | `UrlDecode` 中存在恶意 URL 导致的越界读取风险。 |
| 3 | **中** | HttpConnection.cpp | 服务端 | 逻辑错误 | `FromHex` 在无效输入下使用了未初始化的变量。 |
| 4 | **中** | LogicSystem.cpp | 服务端 | 性能 | IO 线程上的 JSON 解析阻塞了并发处理。 |
| 5 | **低** | ConfigMgr.cpp | 服务端 | 错误处理 | 配置加载错误时的静默失败/返回会导致应用处于无效状态。 |

## 详细分析

### 1. MysqlPool 死锁风险
- **文件:** `server/GateServer/include/MysqlPool.h`
- **位置:** `getConnection()` 方法
- **描述:**
  如果 `MySqlPool` 构造函数未能成功创建连接（例如，由于错误的数据库凭据或 URL），`pool_` 队列将保持为空。构造函数捕获了异常但继续执行。
  后续调用 `getConnection()` 将因为 `pool_` 为空且 `b_stop_` 为 false 而阻塞在 `cond_.wait()` 上。
- **影响:** 服务器线程在尝试访问数据库时将无限期挂起，导致完全拒绝服务。
- **建议:**
  - 如果连接池初始化失败，在构造函数中抛出异常，阻止服务器启动。
  - 或者设置一个标志指示初始化失败，并在 `getConnection` 中进行检查。

### 2. UrlDecode 越界读取
- **文件:** `server/GateServer/src/HttpConnection.cpp`
- **位置:** `UrlDecode` 函数
- **描述:**
  代码检查 `%` 编码：
  ```cpp
  else if (str[i] == '%') {
      assert(i + 2 < length);
      // ...
  }
  ```
  `assert` 在 Release 构建中会被移除。如果用户发送以 `%` 或 `%x` 结尾的 URL，`i+2` 检查将被跳过，`str[++i]` 将读取超出字符串边界的内容。
- **影响:** 崩溃 (Segfault) 或读取敏感内存 (信息泄露)。
- **建议:**
  将 `assert` 替换为运行时检查：
  ```cpp
  if (i + 2 >= length) return ""; // 或处理错误
  ```

### 3. FromHex 未初始化变量
- **文件:** `server/GateServer/src/HttpConnection.cpp`
- **位置:** `FromHex` 函数
- **描述:**
  ```cpp
  unsigned char y;
  if (...) y = ...;
  else assert(0);
  return y;
  ```
  如果输入不是十六进制数字，`y` 在 Release 构建中将未初始化就返回。
- **影响:** 垃圾数据注入到解码后的参数中。
- **建议:**
  返回默认值（如 0）或对无效输入抛出异常。

### 4. IO 线程上的 JSON 解析
- **文件:** `server/GateServer/src/LogicSystem.cpp`
- **位置:** `HandleGet` / `HandlePost` lambdas
- **描述:**
  JSON 解析 (`reader.parse`) 和业务逻辑（RPC 调用、数据库访问）直接在 `HandleReq` 回调中执行。
  由于 `GateServer` 使用单线程 Reactor (`ioc{1}`)，这会阻塞网络线程。
- **影响:** 其他用户的高延迟；在处理繁重请求期间无法接受新连接。
- **建议:**
  将业务逻辑卸载到工作线程池（例如 `AsioIOServicePool` 或专用的逻辑线程池）。

### 5. ConfigMgr 错误处理
- **文件:** `server/GateServer/src/ConfigMgr.cpp`
- **位置:** 构造函数
- **描述:**
  如果 `read_ini` 抛出异常，异常被捕获并记录日志，函数随即返回。应用程序继续运行，但配置为空。
- **影响:** 应用程序使用默认/空值运行，导致后续出现令人困惑的运行时错误（例如端口为 0，数据库主机为空）。
- **建议:**
  让异常传播或调用 `std::terminate()`，以确保应用程序不会在无效配置下启动。
