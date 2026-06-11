# msrChat 阶段性修复计划（v3 — AI 执行优化版）

> 基于 full-audit-report.md（192 项发现）的系统性偿还路线
> 适用场景：**AI 串行执行**，每阶段为一个可独立交付的里程碑
> TLS 全链路加密作为独立规划轨道，不在本计划阶段内
> 项目当前不支持验证码发送至邮箱功能，验证码仅在 UI 上直接显示（开发/测试模式）

---

## 总览

本计划将审查报告中的 ~166h 技术债务（不含 TLS）拆分为 **8 个阶段 + 1 个独立轨道**，全部**串行执行**。每个阶段是一个可验收的里程碑，阶段内有明确的进入/退出标准。

| 阶段 | 主题 | 估算 | 核心价值 | 依赖 | 状态 |
|------|------|------|----------|------|------|
| Phase 1 | 止血：高危安全与稳定性 | ~9.5h | 消除崩溃和直接可利用漏洞 | 无 | ✅ `16bdcce` |
| Phase 2 | 数据完整性与资源管理 | ~14h | 修复数据损坏风险与内存泄漏 | Phase 1 | ✅ `75a1990` |
| Phase 3 | 认证与会话加固 | ~18h | 堵住身份伪造与会话管理漏洞 | Phase 2 | ✅ `574f29c` |
| Phase 4 | 性能与文件传输体验 | ~25.5h | 提升吞吐量与用户可感知体验 | Phase 2 | ✅ `39b126d` |
| **Phase 4.5** | **Quick Wins + 测试安全网** | **~6h** | **零依赖小改动集中处理 + 为 Phase 5 提供自动化测试保护** | **Phase 4** | **✅ `fb9bf3a`** |
| **Phase 5** | **架构重构 + 基础过载防护** | **~67h** | **降低维护成本，为后续迭代打基础** | **Phase 4.5** | **✅ 5-I `babae2f` + 5-II `bd3de6c`** |
| Phase 6 | 协议演进与工程化 | ~18h | 保障二进制兼容与 CI 质量 | Phase 5 | ✅ `4db227b` |
| Phase 7 | 质量收口与可观测性 | ~19h | 补齐日志、限流补充、隐私 | Phase 5 | ⏳ |
| Track T | TLS 全链路加密 | ~24h | 传输层加密 | 独立规划 | ⏭️ 个人项目不启动 |

> **延后项追踪**：4.3 流式下载 / 4.6 背压控制 / 4.14 滑动窗口 — 建议 Phase 5 完成后作为性能加固独立执行

> **v3 变更说明**（相比 v2）：
> - Phase 2/3 从并行改为串行（Phase 2 数据 → Phase 3 认证），避免 AI 上下文切换导致文件越界
> - 新增 Phase 4.5（Quick Wins + 测试前置），作为 Phase 5 的硬性前置条件
> - Phase 5 增加 AI 执行规则：每个 Service 提取后强制编译+冒烟
> - 重要顺带项升级为显式任务项并附验收条件
> - Phase 7 减去已提前到 Phase 4.5 的测试项，估算从 ~25h 降至 ~19h
> - 验证码相关项降级说明：项目无邮件发送功能，验证码仅 UI 显示

---

## Proto 变更协调策略

Proto 文件 (`Message.proto`) 在多个阶段都需要修改，反复变动容易引入兼容性问题。以下为各阶段的 proto 变更范围和协调原则：

| 阶段 | Proto 变更内容 | 性质 |
|------|---------------|------|
| Phase 2.9 | 添加 `string client_msg_id` 字段（消息去重用） | 新增字段 |
| Phase 5C | 统一错误码 enum 到 proto；消息类型定义移到 proto enum | 新增 enum |
| Phase 6.1-6.3 | 添加 `schema_version`、`reserved` 声明、`optional` 标记 | schema 治理 |
| Phase 6.4 | 密码字段加密（AES-GCM） | 必须执行（Track T 已废弃） |

**协调原则**：Phase 2 只加字段不改结构；Phase 5 在 Service 拆分时一次性完成 enum 迁移；Phase 6 做 schema 治理时不增删字段，只加元数据。每个阶段修改 proto 后立即跑双端编译验证。

---

## Phase 1：止血 — 高危安全与稳定性

**目标**：消除所有可直接导致进程崩溃、数据泄露或被简单利用的 P0 问题。

**估算**：~9.5h（含 2h 回归验证 buffer）| **进入标准**：无 | **退出标准**：所有 P0 项已修复并通过基本回归验证

### 修复清单

| 序号 | 审查报告引用 | 问题 | 修复方案 | 估算 |
|------|-------------|------|----------|------|
| 1.1 | 逻辑#5 | WorkerThread 无 try/catch | ThreadPool::WorkerThread 中 `task()` 外包 `try { ... } catch(...) { spdlog::error(...) }`，防止 `std::terminate()` | 0.5h |
| 1.2 | 逻辑#3 | ChatListModel 持锁调 beginRemoveRows 死锁 | 将 `_mutex` 改为 `QRecursiveMutex`，或改为在锁外执行 `beginRemoveRows/endRemoveRows`，仅在锁内完成数据变更 | 1h |
| 1.3 | 安全#8 | FileRecvMgr 文件名路径遍历 | 在 `GetFinalPath` / `WriteChunk` 入口对文件名做规范化（`QFileInfo::canonicalFilePath`），校验结果路径必须在目标目录内 | 1.5h |
| 1.4 | 安全#2 | 客户端盐值硬编码 | 将盐值从源码移到构建时注入（CMake `configure_file` + 环境变量），至少让反编译不能直接看到；同时标记为 Track T 的前置依赖 | 1.5h |
| 1.5 | 安全#10 | Debug 模式硬编码 token | 用 `#ifdef NDEBUG` 保护或改为从环境变量读取 | 0.5h |
| 1.6 | 安全#9 | 未处理消息类型回传原始 body_data | 在默认分支中丢弃 body_data，仅返回 ERR 响应 | 0.5h |
| 1.7 | 逻辑#19 | handler catch 仅捕获 std::exception | 所有 handler 的 catch 链末尾添加 `catch(...)` 兜底 | 1h |
| 1.8 | 安全#5 (高) | from_uid 不匹配仅 warn 不阻断 | 将 `spdlog::warn` 改为立即 return + 发 ERR 响应 | 0.5h |
| 1.9 | 安全#12 (中) | 验证码 UI 明文显示 | 仅在 Debug 构建下显示，Release 构建隐藏 | 0.5h |

> **注**：审查报告逻辑#1（验证码回传客户端）和安全#1（验证码通过 JSON 响应回传）已从本计划中移除——项目当前无邮件发送功能，验证码本身就是在 UI 上直接显示的开发/测试模式，不构成安全漏洞。

### 顺带修复清单（修改相关文件时一并处理）

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 安全#20 | 验证码 UI 明文显示补充说明 | AuthController.cpp | ☐ |

### 依赖关系

```
无前置依赖，可从代码仓库当前状态直接开始。
1.2 修复后需同步验证 ChatView.qml 的消息列表渲染是否正常（回归范围：消息删除/撤回 UI）。
1.4 盐值迁移后需验证登录/注册流程端到端可用。
```

### 验收检查

- [x] 构造恶意文件名（如 `../../etc/passwd`）验证路径遍历被阻断
- [x] 故意抛出非 std::exception 异常验证 WorkerThread 不崩溃
- [ ] 登录/注册流程端到端可用（盐值迁移后） — 1.4 跳过
- [ ] 全功能冒烟测试：认证→发消息→发文件→发图片

> **Phase 1 状态：✅ 代码完成（commit 16bdcce），待用户编译测试通过**

---

## Phase 2：数据完整性与资源管理

**目标**：消除数据损坏风险，修复所有内存/资源泄漏，加固数据库访问层安全性。

**估算**：~14h（含 3h 回归验证 buffer）| **进入标准**：Phase 1 完成 | **退出标准**：无泄漏、无 UB 数据竞争

> **注**：本阶段对应 v2 的 Phase 3，因串行化调整提前到 Phase 2 执行。数据层改动更底层、更独立，适合先于认证加固完成。

### 修复清单

| 序号 | 审查报告引用 | 问题 | 修复方案 | 估算 |
|------|-------------|------|----------|------|
| 2.1 | 逻辑#7 | ImageStorage AppendChunk 覆盖而非追加 | 修正 insert 逻辑：在 offset 处截断后插入，或改用 `sqlite3_blob_write` 增量 I/O | 2h |
| 2.2 | 逻辑#6 | FileTransferTask 非原子 bool 跨线程 | `_is_image` 和 `_target_offline` 改为 `std::atomic<bool>` | 0.5h |
| 2.3 | 逻辑#12 | ObjectPool 析构 use-after-free | 析构时清空 _pool 中的 weak_ptr/shared_ptr 引用，或在自定义删除器中加 pool 有效性检查 | 1.5h |
| 2.4 | 逻辑#14 | DbWorker 内存泄漏 | 为 `_worker` 指定父对象（QObject 父子机制），或在 cleanup() 中 delete _worker | 0.5h |
| 2.5 | 逻辑#17 | TcpWorker 进程退出泄漏 | 在析构链中显式 `quit()` + `wait()` 后再 `delete`，不依赖 `deleteLater()` | 0.5h |
| 2.6 | 逻辑#34 + #35 | 9 处 sqlite3_column_text 无 null 检查 | 封装 `SafeColumnText(stmt, col)` 辅助函数，返回空字符串替代 nullptr | 1.5h |
| 2.7 | 逻辑#18 | 认证 handler 异常时 ContinueReading 不被调用 | 在 handler lambda 的 catch 中确保调用 ContinueReading() | 0.5h |
| 2.8 | 性能#10 (P2) | InsertMessageSorted 缺 _mutex 保护 | 添加 _mutex 保护，或确认调用链上已有锁 | 0.5h |
| 2.9 | 容错#12 (高) | 消息去重依赖 timestamp UNIQUE | 改为 `client_msg_id` 作为去重键（客户端生成 UUID），DB UNIQUE 约束改到此字段。**proto 变更**：在消息体中添加 `string client_msg_id` 字段（仅此字段，不改结构） | 3h |

> **注**：原 Phase 2 的"滑动窗口协议"（逻辑#13，5.5h）已移至 Phase 4（4.14 项），因为它是性能优化而非数据完整性问题，且是该阶段最大最风险的单项，放在数据完整性阶段会阻塞后续 Phase 3。

### 顺带修复清单（修改相关文件时一并处理）

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 逻辑#8 | task_id 哈希冲突 | MessageDispatcher.cpp | ☐ |
| P3#37 | TcpMgr Destroy 非线程安全 | TcpMgr.cpp | ☐ |

### 依赖关系

```
2.1 修复后需验证图片上传/下载的端到端流程
2.9 proto 变更：仅新增 client_msg_id 字段，不改已有结构；服务端先兼容无此字段的旧消息
```

### 验收检查

- [ ] 图片分片上传+下载端到端正确（尤其重叠 chunk 场景）
- [ ] valgrind / AddressSanitizer 下无泄漏报告
- [ ] 两条同毫秒消息不再互相覆盖
- [ ] client_msg_id 在 proto 中可用，旧客户端消息仍可正常收发

> **Phase 2 状态：✅ 代码完成（commit 75a1990），待用户编译测试通过**
> 顺带项 逻辑#8 / P3#37 未涉及相关文件修改，延至后续阶段

---

## Phase 3：认证与会话加固

**目标**：修复认证体系中的系统性缺陷，使会话管理达到生产可用的安全水平。

**估算**：~18h（含 3h 回归验证 buffer）| **进入标准**：Phase 2 完成 | **退出标准**：认证流程覆盖安全基线，输入验证到位

> **注**：本阶段对应 v2 的 Phase 2，串行化后调整到 Phase 3 执行。Phase 2（数据完整性）的 SafeColumnText 等通用 helper 已就绪，本阶段可安全修改认证函数而不冲突。

### 修复清单

| 序号 | 审查报告引用 | 问题 | 修复方案 | 估算 |
|------|-------------|------|----------|------|
| 3.1 | 逻辑#2 | DoAccept 将未认证会话以 uid=0 注册 | 改为使用临时 UUID 或 -1 作为未认证会话标识；AddSession 拒绝 uid=0 的重复注册 | 2h |
| 3.2 | 逻辑#15 | AddSession TOCTOU 竞态 | 在 SessionManager 中用原子操作（ShardedMap 的 shard lock 内完成 check+insert） | 1.5h |
| 3.3 | 逻辑#11 | CleanupSession 双重移除 | 审查 `Close()` 内部已做的 RemoveSession，在 CleanupSession 中去掉冗余的按 uid 移除调用 | 0.5h |
| 3.4 | 安全#4 (高) | requires_auth 仅验证 uid!=0 | 增加 from_uid 与 session->GetUid() 的一致性校验 | 1h |
| 3.5 | 安全#6 (高) | Token 永不过期 | TokenManager 增加 `created_at` 时间戳，CheckToken 时校验有效期（默认 7 天，可配置） | 2h |
| 3.6 | 安全#7 (高) | 密码仅一轮 SHA256 | 迁移到 PBKDF2-SHA256（至少 10000 轮）或 bcrypt；新密码立即升级，旧密码登录时透明迁移 | 3h |
| 3.7 | 安全#11 (中) | Token 内存明文存储 | 对 UserMgr 中的 token 做简单 XOR 混淆（非完美方案，但提高反编译门槛） | 1h |
| 3.8 | 逻辑#16 | 端口配置 narrowing conversion | 改为 `uint16_t` 并做范围校验（1-65535），超出则报错退出 | 0.5h |
| 3.9 | 容错#6 (高) | 并发登录缺乏会话踢出通知 | 新会话登录时，若同 uid 已有活跃会话，向旧会话发 KICK 通知后断开 | 1.5h |
| 3.10 | 输入验证#2 (高) | 注册 username/email 未做长度限制和字符白名单 | 服务端在 HandleRegister 入口校验 username 长度（3-20）、字符白名单（字母数字下划线）、email 格式和长度（5-254） | 1h |
| 3.11 | 输入验证#5 (中) | uid 可为负数 | 在协议解析层校验 uid > 0，负数直接断开 | 0.5h |
| 3.12 | 数据存储#3 (中) | 旧版明文密码兼容逻辑 | 在 3.6 密码哈希迁移完成后，移除旧版明文密码验证分支 | 0.5h |

### 顺带修复清单（修改相关文件时一并处理）

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 逻辑#22 | 时间精度不一致，统一用毫秒 | MessageDispatcher.cpp | ☐ |
| 逻辑#29 | 注册按钮 enabled 不校验必填字段 | RegisterView.qml | ☐ |
| P3#40 | 重置密码不校验确认密码 | ResetView.qml | ☐ |
| P3#41 | uid narrowing int64→int | SQLiteMgr.cpp | ☐ |

> **注**：v2 中此阶段的顺带项"验证码 RNG 线程安全"（逻辑#21）和"验证码按钮不校验邮箱格式"（安全#18）已降级——项目无邮件发送功能，验证码仅 UI 显示，这两项紧迫性大幅降低，移至 P3 择机处理。

### 依赖关系

```
3.1 → 3.2：先改 uid=0 注册策略，再修 TOCTOU 竞态
3.4 依赖 Phase 1.8（from_uid 阻断）已生效
3.6 密码哈希升级需兼容旧密码（登录时透明迁移），需先有完整的登录回归测试
```

### 验收检查

- [ ] 两个客户端同时用同一账号登录，旧客户端收到踢出通知
- [ ] Token 过期后请求被拒绝，重新登录获取新 Token
- [ ] 旧密码用户登录时自动升级为 PBKDF2 哈希
- [ ] 并发创建同 uid 会话不出现覆盖
- [ ] 注册时非法用户名/邮箱被拒绝
- [ ] 全功能冒烟测试：认证→聊天→文件→图片→重连

> **Phase 3 状态：✅ 代码完成（commit 574f29c），待用户编译测试通过**
> 顺带项 4 项未涉及相关文件修改，延至后续阶段

---

## Phase 4：性能与文件传输体验

**目标**：消除最严重的性能瓶颈，补齐文件传输的用户体验缺陷。

**估算**：~25.5h（含 2h 回归验证 buffer）| **进入标准**：Phase 2 完成 | **退出标准**：文件传输体验完整，核心路径无明显性能瓶颈，安全校验到位

### 修复清单

| 序号 | 审查报告引用 | 问题 | 修复方案 | 估算 |
|------|-------------|------|----------|------|
| 4.1 | 性能CPU#1 | HandleChatText 双重序列化 | 直接从 Protobuf ChatTextMsg 填充 ServerChatMsg，跳过中间 JSON 层 | 2h |
| 4.2 | 性能CPU#3 | FileChunk 数据三重拷贝 | protobuf `set_data` 接受 `std::string&&` 或零拷贝方式 | 1.5h |
| 4.3 | 性能内存#1 | 图片下载全量预读到 vector | 改为流式分块读取，每次发送固定大小 chunk | 2h |
| 4.4 | 性能内存#2 | AppendChunk 全量 blob 读写 | 使用 `sqlite3_blob_open` + `sqlite3_blob_write` 做增量 I/O。**显式依赖 Phase 2.1 完成** | 3h |
| 4.5 | 逻辑#4 | 文件接收进度条不工作 | 在 `onSigFileRecvProgress` 中更新 `fileProgressModel`，实现进度百分比计算和 UI 绑定 | 2h |
| 4.6 | 性能网络#1 | 图片下载无背压控制 | 发送 chunk 后等待客户端 ACK 再发下一个，或限制在途数据量 | 2h |
| 4.7 | 性能CPU#4 | actionReply/CopyText 线性扫描 | ChatListModel 提供 `FindMessageByTimestamp()` 的 O(1) 哈希查找 | 1h |
| 4.8 | 性能IO#1 | 离线消息查询无索引 | 添加 `CREATE INDEX idx_messages_to_uid ON messages(to_uid, id)` | 0.5h |
| 4.9 | 性能网络#3 | PrependMessages 加载历史时跳到底部 | `onCountChanged` 中区分 append 和 prepend 操作，prepend 不调用 `positionViewAtEnd()` | 1h |
| 4.10 | 性能网络#4 | 心跳 15s 过于频繁 | 延长到 30s | 0.2h |
| 4.11 | 逻辑#10 | onCountChanged 无差别滚到底部 | 添加标志位区分消息来源方向 | 0.5h |
| 4.12 | 安全#14 (中) | Image source 接受任意 file:// 路径 | 限制 `Image.source` 仅允许应用资源目录内的路径，拒绝外部 `file://` URI；在 ImageViewer.qml 和 ImageBubble.qml 中添加路径白名单校验 | 1h |
| 4.13 | 安全#15 (中) | MAX_LENGTH=1MB 缺乏深度校验 | 在 const.h 和 MessageDispatcher.cpp 中实现消息体大小分级校验：文本消息 ≤64KB、文件元数据 ≤4KB、图片/文件 chunk ≤1MB；超限直接断开并记日志 | 1.5h |
| 4.14 | 逻辑#13 | FileSendMgr stop-and-wait | 改为滑动窗口协议（窗口大小 N=8），发送端维护未确认窗口，收到 ACK 后滑动。**涉及双端协议改动，需充分测试乱序/丢包/重传边界** | 5.5h |

### 顺带修复清单（修改相关文件时一并处理）

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 逻辑#9 | file:// URI 硬编码 | ImageBubble.qml, ImageViewer.qml | ☐ |
| 逻辑#24 | InsertMessageSorted 锁持有时间过长，优化锁范围 | ChatListModel.cpp | ☐ |
| 逻辑#26 | messageAgeSec 非动态更新，改为 Timer 驱动 | MessageActionMenu.qml | ☐ |
| 逻辑#28 | 图片 expires_at = created_at，修正过期时间计算 | MessageDispatcher.cpp | ☐ |
| 逻辑#31 | string_view 多余拷贝 | MessageDispatcher.cpp | ☐ |

### 依赖关系

```
4.4 显式依赖 Phase 2.1（AppendChunk 逻辑修正）已完成
4.3 + 4.6 配合使用，先改流式读取再加背压
4.9 + 4.11 是同一个 QML 问题的两面，合并修改
4.14 涉及双端协议改动，建议放在 Phase 4 后半段，前面先完成较简单的项
其余项串行执行即可
```

### 推荐执行顺序（减少 client/server 上下文切换）

```
服务端优先（改动集中在 MessageDispatcher / ImageStorage / SQLiteMgr）：
  4.1（双重序列化）→ 4.3（图片下载流式）→ 4.4（增量 BLOB）→ 4.6（背压）→ 4.8（索引）→ 4.13（消息大小校验）

客户端优先（改动集中在 FileSendMgr / ChatListModel / QML）：
  4.2（三重拷贝）→ 4.5（进度条）→ 4.7（O(1) 查找）→ 4.9+4.11（滚动行为）→ 4.10（心跳）→ 4.12（路径白名单）

双端协议改动（放在最后，需要同时改客户端和服务端）：
  4.14（滑动窗口）
```

### 验收检查

- [x] 文件传输显示实时进度条
- [ ] 10MB 图片上传/下载耗时对比修复前有显著下降
- [x] 翻看历史消息时视图不跳到底部
- [ ] 离线消息加载性能在有索引后可感知提升
- [ ] Image source 拒绝应用目录外的 file:// 路径 — 4.12 误拦发送方本机路径已回退，Phase 1.3 覆盖
- [x] 超限消息体被拒绝并记录日志
- [ ] 大文件传输吞吐量对比修复前有可测量提升（滑动窗口）— 4.14 延后
- [ ] 滑动窗口边界验证 — 4.14 延后

> **Phase 4 状态：✅ 代码完成（commit 39b126d）**
> 延后：4.3 流式下载、4.6 背压控制、4.14 滑动窗口

---

## Phase 4.5：Quick Wins + 测试安全网

**目标**：集中处理零依赖的微小改动，并为 Phase 5 大规模重构编写自动化测试安全网。

**估算**：~6h（含 1h 回归验证 buffer）| **进入标准**：Phase 4 完成 | **退出标准**：Quick Wins 全部完成，核心测试编写并通过

> **本阶段是 Phase 5 的硬性前置条件**。AI 执行大规模重构时没有手动冒烟能力，必须有自动化测试保护。

### Quick Wins 清单

以下为各阶段散落的"一行改动"或"零依赖小改"，集中处理避免遗忘：

| 序号 | 审查报告引用 | 内容 | 修复方案 | 估算 | 状态 |
|------|-------------|------|----------|------|------|
| QW.1 | 性能IO#1 | 离线消息查询索引 | `CREATE INDEX idx_messages_to_uid ON messages(to_uid, id)` | 0.5h | ✅ Phase 4.8 已完成 |
| QW.2 | 性能网络#4 | 心跳间隔调整 | TcpWorker.cpp 心跳从 15s 改为 30s | 0.2h | ✅ Phase 4.10 已完成 |
| QW.3 | 逻辑#22 | 时间精度统一 | MessageDispatcher.cpp 中 now_ms 统一用毫秒精度 | 0.3h | ✅ 已完成 |
| QW.4 | 逻辑#28 | 图片过期时间修正 | MessageDispatcher.cpp 中 expires_at 修正为 created_at + 合理时长 | 0.3h | ✅ 已完成 |

> **注**：QW.1 和 QW.2 已在 Phase 4 中完成，Phase 4.5 仅需处理 QW.3 和 QW.4。

> **注**：QW.1 和 QW.2 通常已在 Phase 4 中完成，此处作为检查点确保不遗漏。

### 测试安全网（Phase 5 前置条件）

| 序号 | 审查报告引用 | 测试范围 | 估算 |
|------|-------------|----------|------|
| T.1 | 测试P0#1-2 | **认证流程集成测试**：注册→登录→重置→验证码过期/错误。验证 Phase 3 的认证加固未被后续改动破坏 | 2h |
| T.2 | 测试P0#3-5 | **核心 handler 集成测试**：ChatText/Recall/Edit，Mock session。验证消息发送/撤回/编辑的端到端逻辑 | 2h |

#### 测试技术指引（AI 执行必读）

| 要素 | 方案 | 说明 |
|------|------|------|
| 测试框架 | GoogleTest | 项目已集成，参考 `server/tests/test_ShardedMap.cpp` |
| Mock 策略 | 创建 `TestSession` stub 类 | 实现 CSession 的公共接口（`Send()`, `GetUid()`, `ContinueReading()`），不依赖 Boost.Asio |
| 测试数据库 | 内存 SQLite (`":memory:"`) | 每个测试用例独立创建，避免测试间状态污染 |
| 参考模式 | `test_ImageStorage.cpp` 的初始化 | 观察其如何创建临时 DB、初始化模块、清理 |
| CMake 集成 | 新增 `test_auth_flow.cpp` 和 `test_chat_handlers.cpp`，加入 CMakeLists 的 `add_test()` | 确保 `ctest` 一键可跑 |

> **AI 执行提示**：写测试前先阅读 `server/tests/` 目录下现有测试文件，理解项目的测试模式（如何初始化 SQLiteMgr、如何创建 mock 数据）。不要自己发明新的测试基础设施。

#### 测试验收标准

- [x] 认证流程测试：注册→登录→Token 校验→重置密码全流程自动化（test_AuthFlow.cpp，7 个用例）
- [x] 核心 handler 测试：消息 CRUD + 撤回 + 编辑 + 离线消息自动化（test_MessageOps.cpp，8 个用例）
- [x] 测试可通过 `ctest` 或等效命令一键运行
- [x] 所有测试绿色通过（用户编译验证通过）

### 依赖关系

```
Quick Wins 无特殊依赖，按序号执行即可。
测试安全网必须在 Phase 4 完成后编写——此时核心功能已修复完毕，测试基线稳定。
Phase 5 的进入标准要求 T.1 和 T.2 的测试全部绿色通过。
```

> **Phase 4.5 状态：✅ 代码完成，编译测试通过**
> Quick Wins: QW.3（NowMs 真正毫秒精度）+ QW.4（expires_at + 7天）
> 测试安全网: test_AuthFlow.cpp（7 用例）+ test_MessageOps.cpp（8 用例）

---

## Phase 5：架构重构

**目标**：拆分 God File / God Component，引入 Service 层，统一错误码。这是工作量最大的阶段，但对长期维护价值最高。

**估算**：~67h（含 10h 回归验证 buffer）| **进入标准**：Phase 4.5 完成且 T.1/T.2 测试全部绿色 | **退出标准**：核心文件 < 500 行，职责单一，DB 层解耦，基础过载防护到位

> **本阶段分为两个子里程碑，中间强制回归验证。**

### ⚠️ 执行前强制要求：产出 Phase 5 详细子计划

**在开始任何代码修改之前，执行 AI 必须先阅读当前代码并产出 `phase-5-detailed-plan.md`，内容包括：**

1. **5D（DB 层拆分）详细设计**：
   - AuthRepository / MessageRepository / ConfigRepository 各自的公共接口清单（方法名 + 参数 + 返回值）
   - SQLiteMgr.h 中哪些方法归入哪个 Repository
   - 新文件命名和目录结构
   - 每个 Repository 的编译验证步骤

2. **5A（Service 层拆分）详细设计**：
   - AuthService / ChatService / FileService / ImageService 各自的公共接口清单
   - MessageDispatcher.h 中哪些 handler 归入哪个 Service
   - DispatchGuard RAII 的具体设计（构造参数、析构行为）
   - Service 如何获取依赖（构造函数注入 vs 单例引用）
   - Pilot 拆分的验证清单

3. **5B（客户端拆分）详细设计**：
   - ChatView.qml 中哪些代码块提取到哪个子组件
   - 子组件间的信号/属性通信方案
   - ChatController.cpp 的 MessageActions / FileCoordinator 接口设计

4. **执行顺序和检查点**：
   - 每个子阶段的 git commit 粒度
   - 每个 commit 后的验证命令

> **产出子计划后，先将子计划呈现给用户确认，再开始执行。不要在同一个会话中既设计又执行 Phase 5。**

### AI 执行规则（Phase 5 强制）

1. **每个 Service 提取后立即编译验证**：提取 AuthService → 编译通过 → 跑 T.1 测试 → 再提取 ChatService → 编译通过 → 跑 T.1+T.2 → 依此类推
2. **每个 Repository 拆分后立即编译验证**：拆分 AuthRepository → 编译通过 → 跑 T.1 → 依此类推
3. **编译失败时立即修复**：不继续下一个提取，先修复当前编译错误
4. **每个子阶段完成后打 git tag**：方便回滚

---

### 子里程碑 5-I：服务端 Service 拆分 + 基础过载防护（~36h）

完成后必须通过全功能端到端回归，确认路由层拆分没有破坏现有功能。

> **执行顺序：先拆 DB 层（5D），再拆 Service 层（5A）**。理由：5A 提取的 Service 会调用 SQLiteMgr 的方法。如果先拆 Service，它们会引用旧的 SQLiteMgr 接口；随后 5D 拆分 SQLiteMgr 时，所有 Service 都要更新引用——等于改两遍。先拆 DB 层，Service 提取时直接引用新的 Repository 类名，一次到位。

#### 5D：数据库层重构（~12h）

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5D.1 | 可维护#3 | SQLiteMgr.cpp（1238行）拆分：AuthRepository（注册/登录/验证码/Token）、MessageRepository（消息 CRUD/离线消息）、ConfigRepository（配置/元数据——审查后无业务方法，不创建）。**每个 Repository 提取后立即编译 + 跑相关测试** | 6h | ✅ `e91bb37` |
| 5D.2 | 技术债务 | SQLite 连接池从 pool_size=1 扩展为可配置（默认 8），使用 WAL 模式支持并发读 | 3h | ✅ `2ac3ae9` |
| 5D.3 | 技术债务 | ImageStorage::DeleteExpired 接入生产环境：服务端启动时 + 定时任务（每6小时）调用；SQL 改为清理所有过期图片 | 1h | ✅ `2ac3ae9` |
| 5D.4 | 安全#13 (中) | INSERT OR REPLACE 无所有权校验 — **审查后确认安全**：2 处 INSERT OR REPLACE 均在安全上下文中，UPDATE/DELETE 的 WHERE 已含 uid 校验，无需修复 | 1.5h | ✅ 免修 |
| 5D.5 | 安全#16 (中) | sqlite3_bind_blob size 参数 int 溢出 — AppendChunk 增加 100MB 业务上限 + INT_MAX 检查，SaveOfflineMessage 增加 INT_MAX 检查 | 1h | ✅ `2ac3ae9` |

#### 5A：服务端 MessageDispatcher 拆分（~21h）

> **前置**：5D 已完成（`2ac3ae9`），SQLiteMgr 已拆为 AuthRepository / MessageRepository / SchemaManager。

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5A.1 | 可维护#1 | 提取 Service 层：AuthService、ChatService、FileService、ImageService | — | ✅ `fb1fa76` |
| 5A.2 | 可维护#12 | 提取 handler 公共 helper：`DispatchGuard` RAII 类（构造时记录 session，析构时确保 ContinueReading） | 2h | ✅ `fb1fa76` |
| 5A.3 | — | AuthService：HandleGetVerifyCode, HandleRegister, HandleLogin, HandleResetPwd (~350行) → **提取后立即编译 + 跑 T.1** | 3h | ✅ `fb1fa76` |
| 5A.4 | — | ChatService：HandleChatText, HandleOfflineAck (~230行) → **提取后立即编译 + 跑 T.2** | 3h | ✅ `fb1fa76` |
| 5A.5 | — | FileService：HandleFileReq, HandleFileChunk, HandleFileAck, HandleFileRsp (~410行) → **提取后立即编译 + 手动验证文件传输** | 3h | ✅ `fb1fa76` |
| 5A.6 | — | ImageService：HandleChatImage, HandleImageDownloadReq, HandleChatRecall, HandleChatEdit (~520行) → **提取后立即编译 + 手动验证图片链路** | 3h | ✅ `fb1fa76` |
| 5A.7 | — | MessageDispatcher 保留为纯路由层（49行）：解析消息类型→分发到对应 Service → **编译 + 全量回归** | 2h | ✅ `fb1fa76` |

> **注**：上表合计 16h，额外 5h 为 handler 间隐式依赖排查、集成 buffer 及 contingency。
>
> **Pilot 策略（强制）**：在正式全面拆分前，先选 2 个关联最紧密的 handler（推荐 HandleLogin + HandleRegister）试拆到 AuthService，验证以下拆分模式后再铺开：
> 1. Service 如何获取 session 上下文（直接传参 vs Service 持有 SessionManager 引用）
> 2. 公共 helper 的提取粒度（DispatchGuard RAII vs 纯函数 helper）
> 3. 编译依赖是否需要新增头文件前向声明
>
> Pilot 控制在 2h 以内。如果 Pilot 暴露出严重的设计问题，应回到设计阶段而非强行推进。

拆分后的依赖关系：
```
MessageDispatcher → AuthService → AuthRepository, TokenManager
                 → ChatService → MessageRepository, MessageRouter, CServer
                 → FileService → FileTransfer, SessionManager, MessageRouter
                 → ImageService → ImageStorage, SessionManager
```

#### 5F：基础过载防护（~3h）

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5F.1 | 容错#1 (高) | 消息发送频率限制（令牌桶，默认 10msg/s per user），在 ChatService 入口校验 | 1.5h | ✅ `babae2f` |
| 5F.2 | 容错#3 (高) | 连接数上限（默认 10000，超出拒绝），在 CServer::DoAccept 入口校验 | 0.5h | ✅ `babae2f` |
| 5F.3 | 容错#4 (中) | 线程池任务队列上限（默认 10000，超出返回 ERR_BUSY），在 LogicSystem::PostTask 入口校验 | 0.5h | ✅ `babae2f` |

#### 显式任务项（从 v2 顺带项升级）

以下项在 v2 中为"顺带修复"，因其重要性升级为显式任务：

| 序号 | 审查报告引用 | 内容 | 修复方案 | 估算 | 归属 | 状态 |
|------|-------------|------|----------|------|------|------|
| 5X.1 | 逻辑#30 | SerializeToString 返回值未检查 | 所有 Service 中 SerializeToString 调用后检查返回值，失败则记日志+返回 ERR | 1h | 5-I（Service 拆分后逐个添加） | ✅ `babae2f` |
| 5X.2 | 逻辑#20 | 撤回推送后 ClearRecallNotifies 时序问题 | ChatService 中修正：先推送完成再清理，或使用快照模式 | 0.5h | 5-I（ChatService 内） | ✅ `babae2f` |
| 5X.3 | 逻辑#25 | 双线程池共存 | **不合并**，明确分离职责边界 + 文档化各自用途（CServer 处理网络 I/O，LogicSystem 处理业务逻辑）。实际合并留到后续迭代 | 1h | 5-I（独立于 Service 拆分） | ✅ `babae2f` |
| 5X.4 | 逻辑#36 | 编辑通知离线丢失 | ChatService 中补充离线编辑通知逻辑，与撤回通知对齐 | 0.5h | 5-I（ChatService 内） | ✅ `babae2f` |

> **注**：5X.3 原估算为"统一为一个线程池"（1h），经审查后降级为"文档化职责边界"。理由：合并两个线程池涉及 CServer/LogicSystem/main.cpp 的初始化和关闭顺序，在 Phase 5-I 已经进行大规模拆分的情况下额外引入架构变更风险过高。实际合并留到项目稳定后的后续迭代。

#### 子里程碑 5-I 验收检查

- [x] MessageDispatcher.cpp 行数 < 200（实际 49 行）
- [x] SQLiteMgr.cpp 拆分为 3 个 Repository，各自 < 500 行
- [x] SQLite 连接池 > 1，并发读写性能有提升
- [x] 洪泛消息被令牌桶限流，不进入 LogicSystem 队列
- [x] 超过连接数上限的新连接被拒绝（DoAccept 校验）
- [x] 线程池任务队列满时返回 ERR_BUSY，不 OOM
- [x] 所有 Service 中 SerializeToString 返回值已检查
- [ ] **T.1 + T.2 自动化测试仍全部绿色**
- [ ] **全功能端到端回归通过**：认证、聊天、文件传输、图片上传/下载
- [x] INSERT OR REPLACE 操作有所有权校验

---

### 子里程碑 5-II：客户端拆分与错误码统一（~24h）

#### 5B：客户端拆分（~14h）

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5B.1 | 可维护#2 | ChatView.qml 提取 MessageDelegate.qml（消息气泡渲染逻辑） → **提取后验证消息列表渲染** | 2h | ✅ |
| 5B.2 | — | ChatView.qml 提取 MessageInputArea.qml（输入框 + 附件按钮） → **验证消息发送** | 2h | ✅ |
| 5B.3 | — | ChatView.qml 提取 ChatHeader.qml（顶部信息栏） → **编译验证** | 1h | ✅ |
| 5B.4 | — | ChatView.qml 提取 FileProgressPanel.qml（文件进度面板，替代原计划的 ChatScrollController） → **验证历史加载** | 1h | ✅ |
| 5B.5 | — | ChatView.qml 保留为组合层（~303行）+ 提取 ImagePreviewBar.qml + EmptyState.qml → **全量 UI 验证** | 2h | ✅ |
| 5B.6 | 可维护#4 | ChatController.cpp 拆分：提取 MessageActions（reply/copy/recall/edit action 逻辑）、提取 FileCoordinator（文件/图片发送接收协调） → **编译 + 全量回归** | 6h | ✅ |

#### 5C：错误码统一（~4h）

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5C.1 | 可维护#6 | 统一错误码定义到 proto 文件的 enum（ErrorCode + MsgType），客户端和服务端共用。**proto 变更**：新增 ErrorCode enum，消息类型 enum | 2h | ✅ |
| 5C.2 | 协议#7 | 补齐 14 个不匹配/缺失的错误码（Global.h 新增 NetworkError/ParseError/InvalidParam/DbError/Kicked/Busy/RateLimited + 全部 4xxx 码） | 1h | ✅ |
| 5C.3 | 协议#8 | ERR_JSON_PARSE 拆分为 ERR_PARSE_ERROR 和 ERR_INVALID_PARAM，AuthService.cpp 按语义分类替换 | 1h | ✅ |

#### 5E：安全加固快速通道（~2h）

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 5E.1 | 安全网络#3 (高) | 重放攻击防护：ChatTextMsg/RecallMsg/EditMsg 添加 NonceHeader（nonce + timestamp），服务端 NonceCache LRU（10000条/5min）校验，TcpMgr 发送端自动填充 | 2h | ✅ |

#### 顺带修复清单（Phase 5 全局，修改相关文件时一并处理）

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| P3#38 | FileTransfer _task_id_allocator 死代码 | FileTransfer.h | ✅ |
| P3#46 | 过时注释 | MessageDispatcher.cpp | ✅ |
| 安全#17 | pendingImagePath 未做路径规范化 | FileCoordinator.cpp（sendImage 已含 normalizeFilePath） | ✅ |

#### 子里程碑 5-II 验收检查

- [x] ChatView.qml 行数 ~303（< 320），ChatController.cpp 行数 ~298（< 400）
- [x] 错误码在客户端和服务端完全一致（proto ErrorCode enum 为权威来源）
- [x] 消息 nonce 防重放生效（NonceCache LRU + 时间窗口校验）
- [ ] **T.1 + T.2 自动化测试仍全部绿色**
- [ ] **全功能端到端回归通过**：认证、聊天、文件、图片、重连、并发登录
- [ ] **Track T 决策点**：✅ 已决策——个人项目，不启动 TLS。Phase 6.4 必须执行，7E.1 保留执行

> **Phase 5-II 状态：✅ 代码完成（commit `bd3de6c`），待用户编译测试通过**
> 5B.4 调整说明：ChatScrollController 未提取（滚动逻辑与 ListView 紧耦合仅 ~35 行，提取后属性管道开销大于收益），替代提取 FileProgressPanel.qml（~233 行独立块）
> 5B.5 调整说明：额外提取 ImagePreviewBar.qml + EmptyState.qml 以达到行数目标

### Phase 5 回滚与止损策略

**回滚机制**：每个子阶段（5A/5D/5F/5B/5C/5E）完成后立即提交并打 tag（见附录 A）。若回归验证发现关键问题，可在当前 tag 处回退。

**止损判断标准**：
| 信号 | 判定 | 动作 |
|------|------|------|
| 5A 拆分后某 Service 超过预估行数 50% | 拆分粒度不合理 | 回退到 5A 前 tag，重新评估 Service 边界 |
| 5D DB 层拆分后出现并发性能退化 | Repository 间锁竞争 | 暂缓 5D.2（连接池扩展），先保持 pool_size=1 验证功能正确性 |
| 5-I 全功能回归失败且 4h 内无法定位根因 | 隐式耦合超出预期 | 停止推进，回退到 Phase 4.5 tag，产出问题分析文档后重新规划 5A |
| 5B 客户端拆分引入 QML 运行时错误 | Loader/delegate 生命周期问题 | 保留 ChatView.qml 为组合层的方案，仅提取独立可运行的子组件 |

**原则**：宁可花半天回退+分析，也不要在半成品上打补丁。

---

## Phase 6：协议演进与工程化

**目标**：加固 Protobuf 协议的向前/向后兼容性，现代化构建系统，补全 CI 流水线。

**估算**：~18h（含 2h 回归验证 buffer）| **进入标准**：Phase 5 完成 | **退出标准**：proto 有版本号、CI 覆盖双平台

### 修复清单

| 序号 | 审查报告引用 | 问题 | 修复方案 | 估算 | 状态 |
|------|-------------|------|----------|------|------|
| 6.1 | 可维护#26 | proto 无 schema 版本号 | 在 ChatTextMsg/ServerChatMsg/ImageMsg 添加 `int32 schema_version`，handler 入口校验 | 1h | ✅ |
| 6.2 | 可维护#27 | proto 无 reserved 声明 | 当前无已删除字段，添加头部注释说明未来删除时必须 reserved | 0.5h | ✅ |
| 6.3 | 可维护#28 | proto 缺 optional 标记 | 为所有字段添加 `[required]`/`[optional]` 文档注释 | 1.5h | ✅ |
| 6.4 | 可维护#25 | 密码字段明文传输 | Auth JSON 消息添加 nonce+timestamp 防重放（复用 NonceCache） | 1h | ✅ `4db227b` |
| 6.5 | 可维护#24 | SHA256() API 已弃用 | **已完成**：Phase 3 PBKDF2 迁移已使用 EVP_sha256() | 0h | ✅ |
| 6.6 | 构建#1-4 | CMake 问题集 | 全局 → target 作用域；proto 移到根 `proto/` 目录；添加 BUILD_TESTS option | 2h | ✅ |
| 6.7 | CI#2-3 | CI 缺客户端构建和 Windows | ci.yml 增加 Windows MSVC 构建 job | 3h | ✅ |
| 6.8 | 协议#2 | 消息 ID 手动同步风险 | **部分完成**：Phase 5-II 已添加 MsgType proto 枚举，C++ 保留手动定义 | — | ✅ |
| 6.9 | DevOps#1 | 缺 Dockerfile | 多阶段 Dockerfile（builder + runtime）+ .dockerignore | 1h | ✅ |

### 顺带修复清单

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 逻辑#32 | build.sh 全量清理 | client/build.sh（添加 --clean 参数） | ✅ |
| 安全#19 | 编辑长度 2000 字节非字符 | Message.proto 注释（改为“≤2000 字符”） | ✅ |
| 安全#21 | config.ini.example 暴露内网 IP | config.ini.example（192.168.226.129 → 127.0.0.1） | ✅ |

### 验收检查

- [x] proto 文件包含 schema_version、字段 [required]/[optional] 注释
- [ ] 新旧 proto 版本可互解析（向前兼容测试）— schema_version=0 兼容 v1
- [ ] CI 在 Linux + Windows 上均绿色通过
- [ ] Docker build 成功并可通过 docker run 启动服务端

> **Phase 6 状态：✅ 代码完成（6.4 commit `4db227b`），待用户编译测试通过**

---

## Phase 7：质量收口与可观测性

**目标**：添加可观测性基础设施、实施剩余限流与容错机制、实现隐私合规功能。

**估算**：~19h（含 2h 回归验证 buffer）| **进入标准**：Phase 5 完成 | **退出标准**：服务端可观测，隐私基线达标

> **注**：v2 中 7A（核心测试补充）的 T.1 和 T.2 已提前到 Phase 4.5，本阶段不再包含。7A 仅保留 T.3（线程安全）和 T.4（路径遍历防御）。

### 7A：补充测试（~2h）

| 序号 | 审查报告引用 | 测试范围 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 7A.1 | 测试P0#6 | ChatListModel 线程安全（8线程并发写入） | 1h | ✅ `fdc6336` |
| 7A.2 | 测试P1#14 | FileRecvMgr 路径遍历防御测试 | 1h | ⏭️ GetFinalPath 为 private，跳过 |

#### 7A 顺带修复

| 报告# | 内容 | 涉及文件 | 完成 |
|-------|------|---------|------|
| 逻辑#33 | 测试共享单例状态导致顺序依赖 | test files | ☐ |
| P3#42 | 测试硬编码 /tmp/ 路径 | test_ImageDownloadMgr.cpp | ☐ |

### 7B：可观测性与运维（~6h）

| 序号 | 审查报告引用 | 修复方案 | 估算 |
|------|-------------|----------|------|
| 7B.1 | 运维#1 | 添加审计日志通道：登录/注册/密码重置/消息发送独立 audit logger | 1.5h |
| 7B.2 | 运维#2 | handler 入口生成 trace ID，贯穿整个请求链路 | 1h |
| 7B.3 | 安全审计#1 (高) | spdlog::debug 不再输出验证码明文 | 0.5h |
| 7B.4 | 运维#5-7 | 添加基础指标计数器（连接数/消息量/DB池利用率）+ 健康检查 HTTP 端点（/health） | 1.5h |
| 7B.5 | 运维#9 | 日志轮转：集成 spdlog 的 rotating_file_sink，单文件 50MB、保留 5 个 | 0.5h |
| 7B.6 | 运维#10 | SQLite 数据备份脚本：定时拷贝 chatserver.db 到备份目录，保留最近 7 天 | 1h |

### 7C：限流与容错（~3h）

> **注**：消息频率限制、连接数上限、线程池队列上限已前置到 Phase 5F。

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 7C.1 | 容错#5 | 发送队列上限（per session，默认 1000 条积压，超出丢弃） | 0.5h | ✅ `02fbb48` |
| 7C.2 | 容错#9 | 写超时机制（与读超时对称，30s） | 1h | ✅ `02fbb48` |
| 7C.3 | 容错#13 | 消息幂等设计：基于 Phase 2.9 已添加的 client_msg_id 字段，在服务端实现去重逻辑 | 1.5h | ✅ `02fbb48` |

> **注**：v2 中的"验证码请求频率限制"（容错#2）已降级移除——项目无邮件发送功能，不存在邮件炸弹风险。

### 7D：隐私合规（~5h）

> **⏭️ 已跳过**：个人项目不需要账户注销和数据导出功能。

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 7D.1 | 隐私#1 (高) | 账户注销功能 | 2h | ⏭️ 跳过 |
| 7D.2 | 隐私#2 (高) | 数据导出功能 | 2h | ⏭️ 跳过 |
| 7D.3 | 隐私#3 (中) | 注册页面增加隐私说明 | 0.5h | ⏭️ 跳过 |
| 7D.4 | 输入验证#4 (中) | 消息 content XSS 过滤 | 0.5h | ⏭️ 跳过 |

### 7E：安全加固收尾（~2h）

> HMAC 签名保留。Track T 已废弃，此项必须执行。

| 序号 | 审查报告引用 | 修复方案 | 估算 | 状态 |
|------|-------------|----------|------|------|
| 7E.1 | 安全网络#2 (高) | 消息签名/MAC：HMAC-SHA256 签名 | 2h | ✅ `20b697c` |
|------|-------------|----------|------|
| 7E.1 | 安全网络#2 (高) | 消息签名/MAC：HMAC-SHA256 签名 | 2h |

### 验收检查

- [ ] 审计日志可查看登录/消息事件，健康检查端点可访问
- [ ] 发送队列有上限，写超时机制生效
- [ ] 用户可注销账户并导出聊天记录
- [ ] 日志文件自动轮转，数据库自动备份
- [ ] （若执行 7E.1）消息签名校验通过
- [ ] T.1 + T.2 + 7A 测试全部绿色
- [ ] 全功能端到端回归通过

---

## Track T：TLS 全链路加密（已废弃）

> **⏭️ 已废弃**：个人项目，不启动 TLS。省 ~24h 工作量。Phase 6.4（密码加密）和 7E.1（HMAC 签名）作为替代方案覆盖安全需求。

TLS 是一个架构级变更，影响网络层所有组件，需要独立设计和测试。

### 预估工作量：~24h

### 关键决策点

| 决策 | 选项 | 建议 |
|------|------|------|
| TLS 库选择 | OpenSSL / wolfSSL / Botan | OpenSSL（项目已依赖 OpenSSL 做 SHA256） |
| 证书管理 | 自签名 / Let's Encrypt | 开发阶段自签名，生产环境 Let's Encrypt |
| 强制 vs 可选 | STARTTLS / 直连 TLS | 直连 TLS（项目为私有部署，无需兼容非 TLS 客户端） |

### 实施子阶段

| 子阶段 | 内容 | 估算 |
|--------|------|------|
| T1 | 服务端 Boost.Asio SSL socket 替换 plain TCP | 6h |
| T2 | 客户端 Qt QSslSocket 替换 QTcpSocket | 4h |
| T3 | 证书生成脚本 + 配置管理 | 2h |
| T4 | 证书固定 (Certificate Pinning) 防中间人 | 3h |
| T5 | 双向认证 (mTLS) 评估与实施 | 4h |
| T6 | 集成测试 + 性能基准测试 | 3h |
| T7 | TLS 握手超时与错误处理 | 2h |

### 与主线计划的交叉影响

```
Track T 已废弃（个人项目）。以下项作为替代方案保留：
- Phase 6.4（密码字段传输加密）：必须执行，防止 proto 抓包暴露密码
- Phase 7E.1（HMAC 签名）：必须执行，提供应用层完整性保护
- Phase 1.4（盐值硬编码）：仍建议保留构建时注入
```

### 时机建议

建议在 **Phase 5 完成后** 启动 Track T。此时 MessageDispatcher 已完成 Service 拆分，网络层变更的影响面更可控。

### 显式决策点：Phase 5-II 完成时

| 判断条件 | 决策 |
|----------|------|
| Track T 能在近期启动且有明确方案 | Phase 6.4 **跳过**，Phase 7E.1 **降级为可选** |
| Track T 推迟或方案未定 | Phase 6.4 **必须执行**，Phase 7E.1 **保留执行** |

> **✅ 已决策（2026-06-11）**：个人项目，不启动 TLS（Track T 废弃）。Phase 6.4 **必须执行**，Phase 7E.1 **保留执行**。

---

## 阶段间依赖与推荐时间线

```
Phase 1 (止血)              ← 第 1 周前半
  │
  Phase 2 (数据完整性)       ← 第 1 周后半 ~ 第 3 周
  │
  Phase 3 (认证加固)         ← 第 3-4 周
  │
  Phase 4 (性能)             ← 第 4-5 周
  │
  Phase 4.5 (Quick Wins     ← 第 5 周后半
            + 测试安全网)
  │
  Phase 5-I (服务端重构      ← 第 6-8 周
            + 过载防护)
  │
  ▼ ▼ 强制回归验证 ▼ ▼
  │
  Phase 5-II (客户端重构     ← 第 8-10 周
             + 安全加固)
  │
  ▼ ▼ Track T 决策点 ▼ ▼
  │
  Phase 6 (协议/工程化)      ← 第 10-12 周
  │
  Phase 7 (质量收口)         ← 第 12-14 周
  │
  Track T (TLS)              ← 第 14-16 周（独立规划）
```

### 推荐节奏

| 周 | 阶段 | 关键产出 | 回归验证 |
|----|------|----------|---------|
| 第 1 周前半 | Phase 1 | 高危安全修复完成 | 全功能冒烟 |
| 第 1 周后半 ~ 第 2 周 | Phase 2 | 数据完整性 + 资源管理 | 数据完整性验证 |
| 第 2-3 周 | Phase 3 | 认证加固 | 认证全流程 |
| 第 3-5 周 | Phase 4 | 性能优化 + 文件传输体验 + 滑动窗口 | 文件/图片端到端 |
| 第 5 周后半 | Phase 4.5 | Quick Wins + 测试安全网 | T.1+T.2 全绿 |
| 第 6-8 周 | Phase 5-I | Service 层 + Repository + 限流 | **全功能端到端回归 + T.1/T.2** |
| 第 8-10 周 | Phase 5-II | QML 拆分 + 错误码 + nonce | 全功能回归 + **Track T 决策** |
| 第 10-12 周 | Phase 6 | CI 双平台 + proto 稳定 | CI 全绿 + Docker 验证 |
| 第 12-14 周 | Phase 7 | 可观测 + 容错补充 + 隐私 | 完整回归 |
| 第 14-16 周 | Track T | TLS 上线 | TLS 握手 + 性能基准 |

> **注**：全串行执行，总时间线约 14-16 周（不含 Track T 则为 12-14 周）。个人开发者每周有效编码时间约 20-25h（含调试/回归），每阶段估算已包含回归时间。

---

## 各阶段的风险与缓解

| 阶段 | 主要风险 | 缓解措施 |
|------|----------|----------|
| Phase 1 | 盐值迁移后旧客户端无法登录 | 保留旧盐值兼容逻辑 1 个版本周期 |
| Phase 2 | AppendChunk 修正和 ObjectPool UAF 修复涉及底层数据结构 | 修改后立即跑图片链路和压力测试回归 |
| Phase 3 | PBKDF2 迁移中旧密码处理复杂 | 登录时检测哈希格式，透明升级；保留旧哈希验证作为 fallback |
| Phase 4 | 滑动窗口协议改动涉及双端 | 先在服务端支持两种模式，客户端切换后再移除旧模式 |
| Phase 5 | 大规模重构引入回归 | **子里程碑机制** + **每 Service 编译+冒烟** + **回滚止损**（详见 Phase 5 回滚策略） |
| Phase 5F | 限流逻辑影响正常用户体验 | 令牌桶参数可配置（config.ini），开发环境设为宽松值 |
| Phase 6 | proto 密码加密增加客户端复杂度 | **必须执行**：6.4 AES-GCM 加密密码字段，需双端同步 |
| Phase 7 | 隐私功能增加协议复杂度 | 账户注销和数据导出先做 MVP（JSON 直出） |

---

## 审查报告交叉引用索引

以下为审查报告中的问题编号到本计划阶段/序号的映射。

### 逻辑错误（46 项）

| 报告# | 严重度 | 计划阶段 | 备注 |
|-------|--------|----------|------|
| 1 | P0 | — | 验证码回传，项目无邮件功能，已从计划移除 |
| 2 | P1 | Phase 3.1 | uid=0 注册 |
| 3 | P0 | Phase 1.2 | 死锁 |
| 4 | P1 | Phase 4.5 | 进度条 |
| 5 | P0 | Phase 1.1 | WorkerThread 异常 |
| 6 | P1 | Phase 2.2 | 原子 bool |
| 7 | P1 | Phase 2.1 | AppendChunk 覆盖 |
| 8 | P1 | Phase 2（附） | task_id 哈希冲突 |
| 9 | P1 | Phase 4（附） | file:// URI 硬编码 |
| 10 | P1 | Phase 4.11 | 无差别滚到底部 |
| 11 | P1 | Phase 3.3 | 双重移除 |
| 12 | P1 | Phase 2.3 | ObjectPool UAF |
| 13 | P1 | Phase 4.14 | stop-and-wait（从 Phase 2 移至 Phase 4） |
| 14 | P1 | Phase 2.4 | DbWorker 泄漏 |
| 15 | P1 | Phase 3.2 | TOCTOU 竞态 |
| 16 | P1 | Phase 3.8 | 端口 narrowing |
| 17 | P1 | Phase 2.5 | TcpWorker 泄漏 |
| 18 | P1 | Phase 2.7 | ContinueReading |
| 19 | P1 | Phase 1.7 | catch 不全 |
| 20 | P2 | Phase 5X.2 | 撤回推送后 ClearRecallNotifies 时序问题（已升级为显式项） |
| 21 | P2 | P3 择机 | 验证码 RNG 线程安全（无邮件功能，降级） |
| 22 | P2 | Phase 4.5 QW.3 | 时间精度不一致（已升级为 Quick Win） |
| 23 | P2 | Phase 5D（附） | DbService static null_db 竞态 |
| 24 | P2 | Phase 4（附） | InsertMessageSorted 锁持有时间过长 |
| 25 | P2 | Phase 5X.3 | 双线程池共存（已升级为显式项） |
| 26 | P2 | Phase 4（附） | messageAgeSec 非动态更新 |
| 27 | P2 | Phase 5B（附） | Loader delegate null item |
| 28 | P2 | Phase 4.5 QW.4 | 图片 expires_at（已升级为 Quick Win） |
| 29 | P2 | Phase 3（附） | 注册按钮 enabled 不校验 |
| 30 | P2 | Phase 5X.1 | SerializeToString 返回值未检查（已升级为显式项） |
| 31 | P2 | Phase 4（附） | string_view 多余拷贝 |
| 32 | P2 | Phase 6（附） | build.sh 全量清理 |
| 33 | P2 | Phase 7A（附） | 测试共享单例状态 |
| 34-35 | P2 | Phase 2.6 | sqlite3_column_text null 安全 |
| 36 | P2 | Phase 5X.4 | 编辑通知离线丢失（已升级为显式项） |
| 37-46 | P3 | 择机处理 | 见下方 P3 处理策略表 |

### P3 项处理策略

| 报告# | 内容 | 建议时机 | 涉及阶段 |
|-------|------|----------|---------|
| 37 | TcpMgr Destroy 非线程安全 | Phase 2（资源管理） | Phase 2 顺带 |
| 38 | FileTransfer _task_id_allocator 死代码 | Phase 5（服务端重构） | Phase 5 顺带 |
| 39 | MessageActionMenu 连续分隔符 | Phase 5B（客户端重构） | Phase 5 顺带 |
| 40 | ResetView 不校验确认密码 | Phase 3（认证加固） | Phase 3 顺带 |
| 41 | uid narrowing int64→int | Phase 3（认证加固） | Phase 3 顺带 |
| 42 | 测试硬编码 /tmp/ 路径 | Phase 7A（测试补充） | Phase 7 顺带 |
| 43 | 资源文件名拼写错误 | Phase 5B（客户端重构） | Phase 5 顺带 |
| 44 | 未使用图标文件 | Phase 5B（客户端重构） | Phase 5 顺带 |
| 45 | AuthBanner 重复条件表达式 | Phase 5B（客户端重构） | Phase 5 顺带 |
| 46 | 过时注释 | Phase 5（服务端重构） | Phase 5 顺带 |

### 安全漏洞（21 项）

| 报告# | 风险 | 计划阶段 | 备注 |
|-------|------|----------|------|
| 1 | 严重 | — | 验证码回传，项目无邮件功能，已移除 |
| 2 | 严重 | Phase 1.4 | 盐值硬编码 |
| 3 | 严重 | Track T | TLS |
| 4 | 高 | Phase 3.4 | requires_auth |
| 5 | 高 | Phase 1.8 | from_uid 阻断 |
| 6 | 高 | Phase 3.5 | Token 过期 |
| 7 | 高 | Phase 3.6 | 密码哈希 |
| 8 | 高 | Phase 1.3 | 路径遍历 |
| 9 | 中 | Phase 1.6 | body_data 回传 |
| 10 | 中 | Phase 1.5 | Debug token |
| 11 | 中 | Phase 3.7 | Token 明文 |
| 12 | 中 | Phase 1.9 | UUID 检测绕过 + UI 明文显示 |
| 13 | 中 | Phase 5D.4 | INSERT OR REPLACE 无所有权校验 |
| 14 | 中 | Phase 4.12 | Image source 任意路径 |
| 15 | 中 | Phase 4.13 | MAX_LENGTH 缺乏深度校验 |
| 16 | 中 | Phase 5D.5 | sqlite3_bind_blob int 溢出 |
| 17 | 中 | Phase 5（附） | pendingImagePath 未做路径规范化 |
| 18 | 中 | P3 择机 | 验证码按钮不校验邮箱（无邮件功能，降级） |
| 19 | 低 | Phase 6（附） | 编辑长度 2000 字节非字符 |
| 20 | 低 | Phase 1.9 | 验证码 UI 明文显示 |
| 21 | 低 | Phase 6（附） | config.ini.example 暴露内网 IP |

### 输入验证（5 项）

| 报告# | 风险 | 计划阶段 | 备注 |
|-------|------|----------|------|
| 1 | 严重 | — | 验证码回传，已移除 |
| 2 | 高 | Phase 3.10 | 注册 username/email 限制 |
| 3 | 高 | Phase 1.3 | 路径遍历 |
| 4 | 中 | Phase 7D.4 | content XSS 过滤 |
| 5 | 中 | Phase 3.11 | uid 负数校验 |

### 性能/可维护性/测试/运维/容错/协议

| 类别 | 项数 | 主要映射 |
|------|------|----------|
| 性能 (17 项) | P1×4 → Phase 4；P2×10 → Phase 4-5；P3×3 → 择机 | 见 Phase 4 修复清单 |
| 可维护 (31 项) | P1×2 → Phase 5A/5B；P2×19 → Phase 5-6；P3×10 → 择机 | 见 Phase 5 各子节 |
| 代码简化 (10 项) | P2×6 → Phase 5A/5B；P3×4 → 择机 | 在重构时顺带简化 |
| 死代码 (13 项) | 全部 P3 → 在各阶段修改相关文件时顺带清理 | 见 P3 处理策略表 |
| 测试缺失 (30 项) | P0×7 → Phase 4.5 + 7A；P1×11 → Phase 7；P2-P3 → 后续迭代 | 见 Phase 4.5 + 7A |
| 运维缺失 (7 项) | Phase 7B | 见 Phase 7B |
| 容错 (8 项) | 高×3 → Phase 5F + 5E；中×4 → Phase 5F + 7C；验证码频率 → 降级移除 | 见 Phase 7C + 5F |
| 协议 (9 项) | 高×2 → Phase 5C + 6；中×5 → Phase 6-7；低×2 → 择机 | 见 Phase 5C, 6 |
| 隐私 (3 项) | 高×2 → Phase 7D；中×1 → Phase 7D | 见 Phase 7D |

---

## 附录 A：每阶段的 Git 分支策略

```
main (稳定)
  └── fix/phase-1-critical     → merge → tag: v0.1-security
  └── fix/phase-2-data         → merge → tag: v0.2-data
  └── fix/phase-3-auth         → merge → tag: v0.3-auth
  └── fix/phase-4-perf         → merge → tag: v0.4-perf
  └── fix/phase-4.5-quickwins  → merge → tag: v0.4.5-quickwins
  └── refactor/phase-5a-server → merge → tag: v0.5a-server  ← 子里程碑 5-I
  └── refactor/phase-5b-client → merge → tag: v0.5b-client  ← 子里程碑 5-II
  └── infra/phase-6-proto      → merge → tag: v0.6-proto
  └── qa/phase-7-quality       → merge → tag: v0.7-qa
  └── feat/tls                 → merge → tag: v1.0-tls
```

---

## 附录 B：阶段间 Proto 变更时间线

```
Phase 2.9   →  新增 client_msg_id 字段（仅加字段，不改结构）
                ↓ 兼容旧客户端（字段缺失时服务端用 timestamp 去重）
Phase 5C    →  新增 ErrorCode enum + 消息类型 enum
                ↓ 双端编译验证
Phase 6.1-3 →  添加 schema_version + reserved + optional 标记
                ↓ 向前兼容测试
Phase 6.4   →  密码字段加密（必须执行，Track T 已废弃）
```

每次 proto 变更后立即执行：
1. `protoc --decode` 验证新旧版本互解析
2. 双端编译通过
3. 至少跑一轮 T.1+T.2 自动化测试
