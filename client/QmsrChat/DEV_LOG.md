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
  - **CMake 配置修复**：将 `usermgr.cpp/h` 添加到 `CMakeLists.txt` 源文件列表中，解决链接错误。

## 7. 密码重置功能 (Password Reset)

### 7.1 界面实现 (ResetDialog)
- **新建 UI**: 创建了 `resetdialog.ui`，包含用户名、邮箱、验证码、新密码输入框及确认/取消按钮。
- **自定义控件**:
  - `ClickedLabel`: 用于实现可点击的密码可见性切换图标。
  - `TimerBtn`: 用于获取验证码的倒计时按钮。
- **布局优化**: 使用 `QHBoxLayout` 和 `QVBoxLayout` 组合，确保界面在不同分辨率下的整洁性。

### 7.2 逻辑交互
- **验证码获取**:
  - 点击获取按钮后触发邮箱正则校验。
  - 校验通过后发送 HTTP 请求 `/get_varifycode`。
  - 按钮进入倒计时状态，防止重复点击。
- **重置流程**:
  - 校验用户名、邮箱、验证码、新密码格式。
  - 发送 HTTP 请求 `/user_reset_pwd`。
  - 成功后跳转回登录界面 (`switchLogin` 信号)。
- **信号槽连接**:
  - `MainWindow` 连接 `ResetDialog::switchLogin` 信号，实现界面切换。
  - `LoginDialog` 连接 `ClickedLabel::clicked` (忘记密码) 信号，跳转至 `ResetDialog`。

## 8. TCP 连接管理 (TcpMgr)

### 8.1 基础架构
- **单例模式**: 继承 `Singleton` 模板类，确保全局唯一实例。
- **信号槽机制**:
  - 实现了 `sig_con_success` (连接成功/失败) 信号。
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

## 9. 登录回包处理与用户管理

### 9.1 UserMgr 单例实现
- 创建 `UserMgr` 单例类，统一管理用户数据 (uid, name, token)。
- 提供 Set/Get 接口供全局访问。

### 9.2 TcpMgr 消息处理增强
- 引入消息处理器注册机制 `initHandlers` 和消息分发机制 `handleMsg`。
- 注册 `ID_CHAT_LOGIN_RSP` 处理器：
  - 解析服务器返回的 JSON 数据。
  - 登录成功：更新 `UserMgr` 数据，发送 `sig_swich_chatdlg` 信号。
  - 登录失败：发送 `sig_login_failed` 信号。
- 优化 `readyRead` 逻辑，调用 `handleMsg` 替代原有的信号转发。

### 9.3 LoginDialog 错误处理
- 连接 `TcpMgr::sig_login_failed` 信号。
- 实现 `slot_login_failed`：
  - 显示具体错误码提示。
  - 恢复登录按钮可用状态。

### 9.4 协议更新
- `global.h` 新增 `ID_CHAT_LOGIN_RSP` (1006) 枚举值。

## 10. 聊天界面 (ChatDialog) 开发

### 10.1 界面重构 (Pure C++ Implementation)
- **移除 UI 文件**: 完全弃用 `chatdialog.ui`，改用纯 C++ 代码 (`chatdialog.cpp`) 实现布局。
- **仿微信布局**:
  - 主窗口尺寸设定为 1000x700。
  - 左侧 `QListWidget` 联系人列表 (固定宽 250px)。
  - 右侧聊天区域，包含顶部标题栏、中间消息滚动区 (`QScrollArea`)、底部输入区。
- **交互优化**:
  - **键盘监听**: 在 `QTextEdit` 上安装 `EventFilter`，实现 **Enter 发送**，**Ctrl+Enter 换行**。
  - **自动吸底**: 连接滚动条的 `rangeChanged` 信号，确保新消息到达时自动滚动到底部。
  - **布局细节**: 使用 `Stretch Factor` 控制左右比例，设置 `ContentsMargins` 为 0 以消除默认边距。

### 10.2 消息气泡 (ChatBubble)
- **自定义控件**: 创建 `ChatBubble` 类 (继承自 `QWidget`)。
- **自绘样式**:
  - 重写 `paintEvent` 绘制圆角矩形背景。
  - **发送者**: 绿色背景 (`#95ec69`)，右对齐。
  - **接收者**: 白色背景 (`#ffffff`)，左对齐。
- **兜底逻辑**:
  - 实现 `generateDefaultAvatar`，当头像加载失败时，自动绘制纯色圆形背景及名字首字母，防止界面崩溃或显示异常。
- **文本处理**: 支持自动换行 (`setWordWrap(true)`), 限制最大宽度为父窗口的 60%。

### 10.3 构建配置
- 更新 `CMakeLists.txt`:
  - 添加 `chatbubble.cpp/h`。
  - 移除 `chatdialog.ui`。

### 10.5 仿微信界面深度重构
- **模块化拆分**:
  - `ChatSideBar`: 负责左侧会话列表、搜索栏和底部功能栏。
  - `ChatArea`: 负责右侧聊天区域、标题栏和输入框。
  - `SessionItemDelegate`: 自定义绘制会话列表项（圆形头像、消息预览、时间戳）。
- **样式升级**:
  - 更新 `stylesheet.qss`，增加了全局滚动条样式、按钮样式及列表项的交互样式。
  - 消息气泡 (`ChatBubble`) 颜色和样式调整为仿微信风格（绿色/白色）。
- **交互优化**:
  - 主窗口固定 25:75 分栏比例。
  - 搜索栏圆角设计。
  - 聊天输入框支持表情、附件按钮（UI 占位）。

### 10.4 登录体验优化
- **防抖动处理**:
  - 在 `on_login_Button_clicked` 中点击即调用 `enableBtn(false)` 禁用按钮。
  - 在所有失败回调路径 (`slot_login_mod_finish`, JSON 解析失败等) 中调用 `enableBtn(true)` 恢复按钮。

## Bug Fixes

- **TcpMgr 编译错误**:
  - 修复 `static_cast` 转换错误：`QAbstractSocket::SocketError` 函数指针类型不匹配，改为使用 Lambda 表达式直接连接 `errorOccurred` 信号。
  - 修复 `connect` 参数个数错误：Qt 5.15+ 的 `errorOccurred` 信号只有一个参数。
  - 修复 `ReqId` 类型转换错误：增加 `static_cast<uint16_t>` 和 `static_cast<ReqId>`。
  - **CMake 配置修复**：将 `usermgr.cpp/h` 添加到 `CMakeLists.txt` 源文件列表中，解决链接错误。
  - **ErrorCodes 枚举修正**:
    - 在 `global.h` 中添加 `ERR_NETWORK` (1002)。
    - 将 `httpmanagement.cpp` 和 `tcpmgr.cpp` 中引用的 `ERROR_NETWORK` 统一修正为 `ERR_NETWORK`。
    - 将 `httpmanagement.cpp` 和 `tcpmgr.cpp` 中引用的 `ERROR_JSON` 统一修正为 `ERR_JSON`。
  - **头文件缺失修复**: `chatsidebar.cpp` 中缺少 `#include <QLabel>`，导致 `QLabel` 未声明错误。
