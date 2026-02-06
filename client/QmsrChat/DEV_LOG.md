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

