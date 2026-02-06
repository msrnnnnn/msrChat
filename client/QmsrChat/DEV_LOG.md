# 开发日志 - 客户端架构重构与功能增强

## 1. 核心架构重构 (HttpManagement)

针对原有的 HTTP 请求分发机制存在的扩展性问题（依赖 `Modules` 和 `RequestType` 枚举，导致 `if-else` 逻辑膨胀），进行了以下重构：

### 1.1 回调机制引入
- **移除旧逻辑**：移除了基于 `signal-slot` 的模块分发机制，不再需要在 `RegisterDialog` 中注册 HTTP 处理器。
- **引入 Lambda 回调**：在 `PostHttpRequest` 中直接支持 `std::function` 类型的成功 (`success`) 和失败 (`failure`) 回调。
- **生命周期安全**：引入 `QPointer<QObject>` 监视接收者（Receiver）的生命周期。如果在网络请求返回时接收者已被销毁（如用户关闭了对话框），则自动丢弃回调，防止崩溃。

**代码变更**:
- `httpmanagement.h/cpp`: 修改 `PostHttpRequest` 签名，增加回调参数；实现 `QPointer` 检查逻辑。

## 2. 注册模块功能增强 (RegisterDialog)

### 2.1 输入校验系统优化
- **即时校验**：利用 `QLineEdit::editingFinished` 信号，在用户完成输入时立即触发校验，而不是等到点击提交时才检查。
- **错误提示管理**：实现了 `AddTipErr` 和 `DelTipErr` 方法，使用 `QMap<TipErr, QString>` 缓存错误信息。
  - 解决了多个错误同时存在时，消除一个错误导致错误提示栏被清空的问题。
  - 错误提示具有优先级（通过 Map 的 Key 排序），确保界面总是显示最靠前的错误。

### 2.2 验证码获取按钮 (TimerBtn)
- **封装自定义控件**：创建 `TimerBtn` 类（继承自 `QPushButton`）。
- **倒计时逻辑**：实现了点击后 10 秒倒计时功能，倒计时期间按钮禁用，结束后自动恢复。
- **解耦设计**：按钮仅负责倒计时显示，具体的发送请求逻辑通过信号槽在外部实现。

### 2.3 密码可见性切换 (ClickedLabel)
- **自定义交互标签**：创建 `ClickedLabel` 类（继承自 `QLabel`）。
- **状态管理**：引入 `ClickLbState` 枚举 (Normal/Selected)，用于追踪点击状态。
- **动态样式**：
  - 重写 `mousePressEvent`、`enterEvent`、`leaveEvent`。
  - 利用 Qt 的动态属性 (`setProperty("state", ...)`) 配合 QSS 实现图标在不同状态（普通、悬浮、点击、选中）下的自动切换。
- **业务集成**：在注册界面实现了点击“眼睛”图标切换密码输入框明文/密文显示模式的功能。

## 3. UI/UX 体验优化

- **视觉反馈**：
  - 为可点击元素（如密码可见性图标）添加了 `PointingHandCursor` 鼠标手势。
  - 完善了 `stylesheet.qss`，增加了针对不同状态的样式定义。
- **资源管理**：
  - 在 `resources.qrc` 中注册了新的图标资源路径。
  - 创建了 `res` 目录用于存放 UI 资源文件（注：目前为占位文件，需替换为真实 PNG）。

## 4. 构建系统更新

- **CMakeLists.txt**：添加了新创建的源文件 `timerbtn.cpp/h` 和 `clickedlabel.cpp/h`，确保项目编译正常。

---

## 5. 文件变更列表

| 文件 | 类型 | 描述 |
| :--- | :--- | :--- |
| `httpmanagement.h/cpp` | 修改 | 重构 HTTP 请求逻辑，支持 Lambda 回调与生命周期保护。 |
| `registerdialog.h/cpp` | 修改 | 接入新的 HTTP 接口；实现输入校验；集成 TimerBtn 和 ClickedLabel。 |
| `registerdialog.ui` | 修改 | 提升控件为 TimerBtn 和 ClickedLabel；调整布局。 |
| `timerbtn.h/cpp` | **新增** | 倒计时按钮控件实现。 |
| `clickedlabel.h/cpp` | **新增** | 可点击标签控件实现（密码显示切换）。 |
| `global.h` | 修改 | 新增 `TipErr` 和 `ClickLbState` 枚举。 |
| `style/stylesheet.qss` | 修改 | 新增图标状态样式定义。 |
| `resources.qrc` | 修改 | 注册新的资源文件路径。 |
| `CMakeLists.txt` | 修改 | 更新文件列表。 |

## 6. 错误修复 (Bug Fixes)

- **编译错误修复**：
  - 修正了 `registerdialog.cpp` 中使用了未定义的枚举 `ReqId` 和 `ErrorCodes` 的问题。
  - 将 `ReqId::ID_REG_USER` 移除，适配新的 `PostHttpRequest` 回调接口。
  - 将 `ErrorCodes::SUCCESS` 修正为 `ERRORCODES::SUCCESS`，并统一使用 `static_cast<int>` 进行比较。

## 7. 密码重置功能 (Password Reset)

### 7.1 界面实现 (ResetDialog)
- **新建 UI**: 创建了 `resetdialog.ui`，包含用户名、邮箱、验证码、新密码输入框及确认/取消按钮。
- **自定义控件**:
  - 集成了 `TimerBtn` (verify_btn) 用于获取验证码倒计时。
  - 使用 `QLineEdit` 并设置 `PasswordEchoOnEdit` 模式用于新密码输入。

### 7.2 登录界面更新 (LoginDialog)
- **标签升级**: 将“忘记密码”标签 (`forget_password_label`) 升级为自定义的 `ClickedLabel`。
- **交互增强**:
  - 设置了标签的正常、悬浮、点击等状态样式。
  - 添加了鼠标手势 (`PointingHandCursor`)。
  - 绑定了点击信号 `ClickedLabel::clicked` 到 `slot_forget_pwd` 槽函数。
- **信号转发**: 在 `slot_forget_pwd` 中发射 `switchReset` 信号，通知主窗口切换界面。

### 7.3 主窗口逻辑 (MainWindow)
- **界面切换**:
  - 实现了 `slotSwitchReset` 槽函数，用于隐藏登录界面并显示重置密码界面。
  - 实现了 `slotSwitchLogin2` 槽函数，用于从重置密码界面返回登录界面。
- **动态加载**: `ResetDialog` 采用懒加载模式，在首次点击“忘记密码”时才进行实例化，节省资源。

### 7.4 业务逻辑实现 (ResetDialog Logic)
- **架构适配**: 将教程中的 `HttpMgr` 替换为项目现有的 `HttpManagement` 单例，并使用 Lambda 回调替代旧的 `initHandlers` 映射表模式，代码更简洁。
- **输入校验**: 实现了用户名、邮箱、密码格式（正则）、验证码的即时校验 (`editingFinished`)。
- **验证码获取**:
  - 集成 `TimerBtn`，点击获取后自动倒计时。
  - 优化体验：若校验失败或网络请求失败，自动停止倒计时并恢复按钮状态，允许用户立即重试。
- **接口对接**:
  - 新增 `ID_RESET_PWD` (1003) 请求类型。
  - 实现 `/get_varifycode` 和 `/reset_pwd` 接口调用。
- **统一规范**: 
  - 移除教程中的 `xorString`，保持与 `RegisterDialog` 一致的明文传输（或由 HTTPS 保证安全）。
  - 统一使用 `ui->error_label` 显示错误提示，并支持 `repolish` 刷新样式。

### 7.5 服务端密码重置支持 (Server Side)
- **路由注册**: 在 `LogicSystem` 中注册 `/reset_pwd` POST 路由。
- **流程实现**:
  - 校验 Redis 中的验证码是否有效及匹配。
  - 校验 MySQL 中用户名与邮箱是否匹配 (`CheckEmail`)。
  - 更新用户密码 (`UpdatePwd`)。
- **数据库优化**:
  - 在 `MysqlDao` 中实现 `CheckEmail` 和 `UpdatePwd`。
  - 使用 RAII (`std::unique_ptr`) 管理数据库连接，替代教程中的手动 `returnConnection`，防止资源泄漏。
- **错误码扩展**: 在 `const.h` 中补充 `EmailNotMatch`, `PasswdUpFailed` 等错误码。

## 8. TCP 连接管理与登录集成 (TcpMgr & LoginDialog)

### 8.1 TcpMgr 单例实现
- **单例模式**: 继承自 `Singleton<TcpMgr>` 和 `std::enable_shared_from_this`，确保全局唯一且生命周期安全。
- **信号槽机制**:
  - 使用 Qt 5.15+ 兼容的 `errorOccurred` 信号替代已弃用的 `error` 信号。
  - 实现 `slot_tcp_connect` 连接服务器。
  - 实现 `slot_send_data` 发送数据，采用 BigEndian 网络字节序，并增加消息头（ID + Length）解决粘包问题。
  - 实现 `readyRead` 处理逻辑，解析消息头和消息体。
- **线程安全**: 通过 `sig_send_data` 信号中转发送请求，支持多线程调用。

### 8.2 LoginDialog 集成
- **HTTP 登录回调**:
  - 实现 `initHttpHandlers` 注册登录成功后的回调逻辑。
  - 登录成功后解析服务器返回的 `ServerInfo` (uid, host, port, token)。
  - 发送 `sig_connect_tcp` 信号触发 TCP 连接。
- **TCP 连接处理**:
  - 连接成功后 (`slot_tcp_con_finish`)，自动构造 ChatLogin 请求并通过 TcpMgr 发送。
  - 界面交互优化：登录过程中禁用按钮 (`enableBtn(false)`)，失败或完成时恢复。
- **代码修正**:
  - 修正了 `ReqId` 枚举值不匹配问题 (`ID_LOGIN_USER` -> `ID_USER_LOGIN`)。
  - 修正了 TCP 错误处理的信号连接语法。
