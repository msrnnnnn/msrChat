# -*- coding: utf-8 -*-
"""
根据模板生成三份实验报告 docx。
模板：综合性和设计性实验报告模板.docx（5段式）
内容来源：旧版 md 报告
风格：叙述式（参考图片风格），不用代码块/markdown 语法
"""

import copy
from docx import Document
from docx.shared import Pt, Cm
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

TEMPLATE = r"C:\OneDrive\Desktop\综合性和设计性实验报告模板.docx"
OUT_DIR = r"E:\Study\Project\Chat\msrChat\reports"

# ── 辅助函数 ──────────────────────────────────────────────

def clear_cell_keep_structure(cell):
    """清空单元格内容，保留第一个段落"""
    for p in cell.paragraphs[1:]:
        p._element.getparent().remove(p._element)
    cell.paragraphs[0].clear()


def add_para(cell, text, style="Normal", bold=False, font_size=None, space_after=None, space_before=None):
    """向单元格追加段落"""
    p = cell.add_paragraph(text, style=style)
    if bold or font_size:
        for run in p.runs:
            if bold:
                run.font.bold = True
            if font_size:
                run.font.size = Pt(font_size)
    if space_after is not None:
        p.paragraph_format.space_after = Pt(space_after)
    if space_before is not None:
        p.paragraph_format.space_before = Pt(space_before)
    return p


def add_heading(cell, text, level=1):
    """添加加粗标题段落"""
    sizes = {0: 16, 1: 14, 2: 12, 3: 11}
    size = sizes.get(level, 12)
    p = add_para(cell, text, bold=True, font_size=size, space_after=4, space_before=8)
    return p


def add_text(cell, text, font_size=10.5):
    """添加正文段落"""
    return add_para(cell, text, font_size=font_size, space_after=2)


def add_list_item(cell, text, font_size=10.5):
    """添加列表项"""
    return add_para(cell, text, font_size=font_size, space_after=1)


def fill_header(doc, name, xuehao, kecheng, xiangmu, laoshi):
    """填充表头信息"""
    table = doc.tables[0]
    # R0: 院系 | 学号 | 姓名 | 成绩
    table.rows[0].cells[3].text = xuehao
    table.rows[0].cells[5].text = name
    # R1: 课程名称 | 实验项目名称 | 指导老师
    table.rows[1].cells[1].text = kecheng
    # R1C2-C4 is merged with gridSpan=3
    table.rows[1].cells[2].text = xiangmu
    table.rows[1].cells[7].text = laoshi


def fill_content(cell, sections):
    """
    填写内容格。
    sections = [(标题, [段落列表]), ...]
    """
    # 清空模板占位
    clear_cell_keep_structure(cell)

    for title, paragraphs in sections:
        add_heading(cell, title, level=1)
        for item in paragraphs:
            if item.startswith("###"):
                add_heading(cell, item.lstrip("#").strip(), level=3)
            elif item.startswith("##"):
                add_heading(cell, item.lstrip("#").strip(), level=2)
            elif item.startswith("- "):
                add_list_item(cell, "  ● " + item[2:])
            elif item.startswith("  - "):
                add_list_item(cell, "    ○ " + item[4:])
            else:
                add_text(cell, item)


def generate_report(out_name, name, xuehao, kecheng, xiangmu, laoshi, sections):
    """生成一份报告"""
    doc = Document(TEMPLATE)
    fill_header(doc, name, xuehao, kecheng, xiangmu, laoshi)
    table = doc.tables[0]
    content_cell = table.rows[2].cells[0]
    fill_content(content_cell, sections)
    path = f"{OUT_DIR}\\{out_name}"
    doc.save(path)
    print(f"  -> {path}")


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 报告1：客户端编程实现
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CLIENT_SECTIONS = [
    ("一、实验目的", [
        "1. 掌握 Qt6/QML 桌面应用程序的开发方法与架构设计，理解 C++ 业务层与 QML 界面层的分离协作机制。",
        "2. 掌握 TCP 网络编程（QTcpSocket）、多线程（QThread）与异步通信的实现原理。",
        "3. 学习环形缓冲区（RingBuffer）解决 TCP 粘包/拆包问题的设计与实现。",
        "4. 掌握 Protocol Buffers 序列化、SQLite 本地持久化等核心技术在客户端中的应用。",
        "5. 掌握文件/图片传输、消息撤回与编辑等即时通讯客户端核心功能的实现。",
    ]),
    ("二、实验原理", [
        "## 2.1 三层分离架构",
        "本客户端采用 QML 界面层、C++ 业务控制器层、C++ 网络通信层的三层分离架构。QML 层只负责界面展示与用户交互，不包含任何业务逻辑；C++ 业务层通过 Q_PROPERTY、Q_INVOKABLE 和信号槽机制向 QML 暴露接口；网络通信层在独立线程中运行，通过 Qt 的信号槽机制实现与主线程的安全通信。这种设计保证了各层职责单一、松耦合协作。",
        "## 2.2 信号槽跨线程通信",
        "Qt 的信号槽机制天然支持跨线程安全通信。当信号接收者与发送者位于不同线程时，Qt 自动使用 QueuedConnection 方式，将槽函数调用投递到接收者所在线程的事件循环中执行。本项目中 TcpWorker 运行在独立 QThread 中，通过 moveToThread 方式迁移，其产生的信号通过信号链逐层传递：TcpWorker（网络线程）→ TcpMgr（主线程）→ AuthController/ChatController（主线程）→ QML 界面（主线程渲染）。",
        "## 2.3 自定义二进制帧与粘包处理",
        "TCP 协议是面向字节流的，不保证消息边界。本项目采用自定义二进制帧格式：消息 ID（2 字节，uint16）+ 消息体长度（4 字节，uint32）+ Protobuf 消息体（变长，最大 1MB），头部固定 6 字节，小端序。配合环形缓冲区（RingBuffer）解决粘包/拆包问题。RingBuffer 初始容量 64KB，按需自动扩容至最大 4MB。解析时先读取 6 字节头部，再根据 body_len 读取完整消息体，不足则等待更多数据到达。这种设计使客户端能够正确处理 TCP 传输中可能出现的多包粘连和单包拆分问题。",
        "## 2.4 Protobuf 序列化协议",
        "采用 Google Protocol Buffers 作为消息序列化格式。相比 JSON，Protobuf 采用二进制编码，体积小 3-10 倍，且具有强类型 schema，可在编译期检查消息结构。每种消息类型对应一个 msg_id（如 1006 对应文本消息，1009 对应图片消息），接收方根据 msg_id 反序列化为对应的 Protobuf 消息对象。",
        "## 2.5 Model-View 编程模型",
        "消息列表采用 QAbstractListModel 作为数据模型（ChatListModel），为 QML 的 ListView 提供数据。模型定义了 14 种角色（ContentRole、FromUidRole、IsSelfRole、TimestampRole、StatusRole 等），支持增删改查操作，并通过 beginInsertRows/endInsertRows 等方法通知视图刷新，实现了数据与界面的完全解耦。",
    ]),
    ("三、实验内容", [
        "## 3.1 网络通信层实现",
        "TcpMgr 是客户端网络层的核心单例类，负责管理 TCP 连接的生命周期。它内部创建 TcpWorker 并将其移到独立 QThread 中运行，对外暴露各种发送接口（如 slot_send_login_req、slot_send_chat_text 等），同时接收 TcpWorker 的信号并转发给上层业务模块。TcpWorker 封装了 QTcpSocket，实现了数据的异步读写，包括心跳保活（15 秒 ping/pong）和指数退避断线重连（3 秒起步，翻倍递增至最大 60 秒）。TcpProtocolParser 负责按 msg_id 将原始数据分发到对应的反序列化处理函数，共支持 15 种消息类型的解析。",
        "## 3.2 业务控制器层实现",
        "AuthController 向 QML 暴露认证相关接口，协调登录、注册、密码重置的完整业务流程。登录采用两阶段方案：第一阶段密码验证获取 Token，第二阶段携带 Token 进行聊天会话认证，认证成功后销毁认证窗口并创建聊天窗口。",
        "ChatController 管理所有聊天相关操作，包括文本消息发送（生成 UUID 标识的 client_msg_id，通过 PendingMessages 跟踪确认状态）、图片发送（提取元数据后上传）、文件发送（分块传输 + 逐块确认）、消息撤回（发送 RecallMsg 到服务端验证）和消息编辑（发送 EditMsg 到服务端更新）。",
        "ChatListModel 继承 QAbstractListModel，定义了 14 种数据角色，支持 AddMessage、InsertMessageSorted（按时间戳排序，用于离线消息）、MarkRecalled、UpdateContent、UpdateStatus 等操作。",
        "## 3.3 文件与图片传输实现",
        "FileSendMgr 实现文件分块发送：打开文件获取大小和 MD5 摘要，发送 FileReq 请求，收到响应后从 offset 位置开始以 4KB 为单位逐块发送，每块等待 ACK 确认。FileRecvMgr 实现文件分块接收：创建临时文件，按 offset 位置写入数据块，传输完成后计算 MD5 校验，通过则重命名为正式文件。",
        "ImageDownloadMgr 实现两级缓存（内存 + 磁盘）的图片下载管线，维护下载队列避免并发过多，失败最多重试 3 次。",
        "## 3.4 本地数据库实现",
        "DbService 以单例模式提供消息 CRUD 接口，内部使用 DbWorker 在独立 QThread 中执行 SQL 操作，避免阻塞 UI 线程。核心操作包括：保存消息、加载历史记录（按时间戳倒序分页查询）、更新发送状态、标记撤回和编辑等。",
        "## 3.5 QML 界面层实现",
        "采用双窗口架构：AuthWindow（无边框 376×540 像素，紫色渐变主题，StackView 管理登录/注册/重置密码三个页面）和 ChatWindow（920×660 像素）。ChatView 是核心聊天视图，包含消息列表、文字气泡（己方蓝色右对齐，对方白色左对齐）、图片气泡（缩略图 + 加载占位）、图片预览条、输入区与工具栏、文件传输进度面板、全屏图片查看器（支持缩放/旋转/翻页）、编辑消息对话框和右键操作菜单（回复/复制/撤回/编辑/删除）。main.cpp 中通过 rootContext->setContextProperty 将 C++ 对象注入 QML 上下文。",
    ]),
    ("四、实验过程原始记录（数据，图表，计算等）", [
        "## 4.1 核心类一览",
        "客户端共实现 12 个核心类：TcpMgr（单例，TCP 连接管理）、TcpWorker（QThread，实际 TCP 读写）、TcpProtocolParser（协议解析分发）、RingBuffer（环形缓冲区 64KB-4MB）、AuthController（QML 认证控制器）、ChatController（聊天控制器）、ChatListModel（QAbstractListModel 消息模型）、UserMgr（单例，用户 UID/Token 存储）、DbService（单例，SQLite CRUD）、DbWorker（QThread，数据库工作线程）、FileSendMgr（单例，文件发送 64KB 分片）、FileRecvMgr（单例，文件接收 + MD5 校验）、ImageDownloadMgr（单例，图片下载队列 + 缓存 + 重试）。",
        "## 4.2 线程模型",
        "客户端运行三个线程：主线程负责 Qt 事件循环和 QML UI 渲染；网络线程运行 TcpWorker，执行 QTcpSocket 异步读写和心跳重连；数据库线程运行 DbWorker，执行 SQLite 异步读写。所有跨线程通信均通过 Qt 信号槽机制保证线程安全。",
        "## 4.3 测试结果",
        "使用 Google Test 框架进行了单元测试，覆盖以下模块：",
        "- test_RingBuffer.cpp：测试环形缓冲区的 Push/PopPacket 操作、自动扩容、边界条件处理，全部通过。",
        "- test_ChatListModel.cpp：测试消息列表模型的 AddMessage、MarkRecalled、角色访问等操作，全部通过。",
        "- test_ImageDownloadMgr.cpp：测试图片下载管线的缓存命中、重试机制、队列管理，全部通过。",
        "## 4.4 构建验证",
        "使用 CMake 3.16+ 构建，依赖 Qt6（Network、Core、Quick、Qml、Sql、QuickControls2、QuickLayouts）和 Protobuf 3.x。在 Windows（MSVC）和 Linux（GCC）环境下均编译通过，Debug 模式启用 AddressSanitizer 进行内存检查。",
    ]),
    ("五、实验结果分析或总结", [
        "通过本次实验，成功实现了基于 Qt6/QML 的即时通讯桌面客户端，主要收获如下：",
        "在架构设计方面，深入理解了 C++ 业务层与 QML 界面层的分离设计原理。通过 context property 和信号槽机制实现了双向通信，使业务逻辑与界面展示完全解耦，便于独立开发和测试。",
        "在网络编程方面，掌握了 QTcpSocket 在独立线程中的使用方法，理解了 moveToThread 加信号槽的跨线程通信模式。通过自定义 RingBuffer 环形缓冲区实现了高效的 TCP 粘包处理，支持自动扩容和最大容量保护（64KB 至 4MB）。",
        "在协议设计方面，掌握了 Protocol Buffers 在 C++ 项目中的集成使用。自定义二进制帧格式（6 字节头部 + Protobuf 消息体）兼顾了解析效率和可扩展性，共支持 15 种消息类型。",
        "在多线程架构方面，建立了主线程（UI）+ 网络线程 + 数据库线程的三线程模型，通过 Qt 信号槽实现安全的跨线程通信，有效避免了 UI 阻塞和数据竞争问题。",
        "在数据持久化方面，使用 SQLite 配合独立 DbWorker 线程实现了异步消息存储，支持消息的增删改查、状态管理和历史加载。",
        "在 UI 设计方面，纯 QML 实现了现代化聊天界面，包括消息气泡、图片预览与全屏查看、右键操作菜单、文件传输进度面板等交互组件，用户体验良好。",
        "不足之处在于：当前客户端仅支持单聊场景，群聊功能尚未实现；图片传输未采用压缩优化，大图传输效率有待提升；部分 UI 组件的触屏适配还需进一步完善。",
    ]),
]


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 报告2：服务端（后端）编程实现
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

BACKEND_SECTIONS = [
    ("一、实验目的", [
        "1. 掌握基于 Boost.Asio 的高并发 TCP 服务器编程模型，理解异步 I/O 的核心原理。",
        "2. 理解线程池、连接池等后端核心技术的设计与实践。",
        "3. 掌握 Protocol Buffers 序列化协议在即时通讯系统中的应用。",
        "4. 学习 SQLite 数据库连接池设计与持久化存储方案。",
        "5. 理解消息分发、路由、离线存储等即时通讯服务端核心架构。",
    ]),
    ("二、实验原理", [
        "## 2.1 Boost.Asio 异步 I/O 模型",
        "Boost.Asio 是基于 Proactor 模式的异步 I/O 框架。其核心组件 io_context 提供事件循环，所有异步操作（如 async_accept、async_read、async_write）提交后立即返回，由框架在 I/O 完成时调用注册的回调函数。这种非阻塞模型允许单个线程同时管理大量并发连接，相比传统的线程池阻塞模型，资源利用率更高。",
        "## 2.2 Boss-Worker 线程池模型",
        "本项目采用 Boss-Worker 模式的 I/O 线程池。Boss 线程运行主 io_context，负责接受新连接；Worker 线程各自运行独立的 io_context，负责已建立连接的读写操作。新连接通过轮询（Round-Robin）策略分配到不同 Worker，实现多核 CPU 负载均衡。业务逻辑通过独立的 LogicSystem 线程池处理，与网络 I/O 完全解耦。",
        "## 2.3 strand 串行化保证",
        "在多线程环境下，同一连接的异步操作可能在不同线程中执行回调，导致数据竞争。Boost.Asio 提供 strand 包装器，保证同一 strand 内的回调函数串行执行，无需额外加锁。本项目中每个 CSession 使用独立的 strand 包装所有异步读写操作。",
        "## 2.4 分片锁并发数据结构",
        "传统的全局互斥锁在高并发场景下会成为性能瓶颈。本项目设计了 ShardedMap 分片哈希表，将键空间划分为 32 个分片，每个分片独立持有互斥锁。不同分片的读写操作可以完全并行，理论上将锁竞争降低到单锁方案的 1/32。",
        "## 2.5 数据库连接池与 WAL 模式",
        "SQLite 是嵌入式数据库，默认采用文件锁保证并发安全，但写操作会阻塞读操作。本项目启用 WAL（Write-Ahead Logging）模式，允许读写并发执行。同时设计了 8 连接的连接池，通过 RAII 模式（std::unique_ptr + 自定义 Deleter）管理连接生命周期，获取时从池中取出，归还时自动放回。",
    ]),
    ("三、实验内容", [
        "## 3.1 网络接入层 — CServer",
        "CServer 是服务端的入口类，负责 TCP 连接接入。创建 boost::asio::ip::tcp::acceptor 监听指定端口，通过 async_accept 异步接受新连接，将新会话分配到 AsioIOServicePool 的不同 io_context 中。使用 shared_from_this 保证对象生命周期安全。支持优雅关闭：注册 SIGINT/SIGTERM 信号处理器，收到信号后停止 acceptor 并等待所有会话关闭。",
        "## 3.2 I/O 线程池 — AsioIOServicePool",
        "维护 N 个 boost::asio::io_context（N 默认为 CPU 核心数），每个 io_context 绑定一个独立线程运行 io_context::run()。新连接通过轮询分配到不同 io_context。采用单例模式（CSingleton），全局共享。每个 io_context 上运行多个 CSession 对象。",
        "## 3.3 会话管理 — CSession",
        "每个 TCP 连接对应一个 CSession 对象，是服务端最核心的类之一。协议解帧流程为：AsyncReadHead 读取 6 字节头部（2 字节消息 ID + 4 字节消息体长度），AsyncReadBody 根据长度读取 Protobuf 消息体，HandleMessage 投递到 LogicSystem 异步处理，然后继续循环读取下一条。使用 boost::asio::strand 保证同一 Session 的读写串行执行，使用 std::atomic<bool> 防止并发读写和重复登录。实现 30 秒读超时检测（5 秒检查间隔）。异步发送队列缓存待发送数据，通过 strand 保证串行发送。",
        "## 3.4 业务调度层 — LogicSystem",
        "LogicSystem 是业务逻辑的入口，采用生产者-消费者模型。CSession 接收完整消息后构造 MessageTask 对象，调用 LogicSystem::PostTask() 将任务放入队列，内部 ThreadPool 的 Worker 线程从队列中取出任务，根据 msg_id 调用 MessageDispatcher::Dispatch() 分发到对应 Handler。这种设计将网络 I/O 和业务逻辑完全解耦。",
        "## 3.5 消息分发器 — MessageDispatcher",
        "采用注册表模式（Registry Pattern），启动时注册所有 msg_id 到 Handler 的映射。运行时只读查表，无需加锁，查表时间复杂度 O(1)。支持鉴权检查：对需要登录态的消息自动检查 Session 是否已完成 Token 认证。共注册了 14 个消息处理器，覆盖心跳、认证、聊天、图片、撤回编辑、文件传输等全部业务。",
        "## 3.6 消息路由 — MessageRouter",
        "负责消息的在线转发和离线存储。转发逻辑为：收到消息后通过 SessionManager 查询目标用户是否在线，在线则通过 CSession::Send() 直接转发，离线则调用 SQLiteMgr::SaveOfflineMessage() 存入离线消息表。使用全局递增的 server_msg_id 确保消息全局唯一有序。在线转发时同时回送 ChatAck 给发送方确认送达。离线消息在用户上线后分页发送（每页 50 条）。",
        "## 3.7 会话管理器 — SessionManager",
        "管理所有在线会话，使用 ShardedMap（32 分片）实现高并发读写。维护双映射：uid 到 session 的映射用于消息路由，uuid 到 session 的映射用于连接管理。",
        "## 3.8 数据库管理 — SQLiteMgr",
        "SQLite 数据库连接池，维护 8 个 SQLite 连接。Acquire() 获取连接，Release() 归还连接，使用 std::unique_ptr 加自定义 Deleter 实现 RAII 自动归还。数据库启用 WAL 模式支持并发读写。核心业务接口包括 LoginUser、RegisterUser、SaveMessage、SaveOfflineMessage、LoadOfflineMessages、MarkRecalled、MarkEdited 等。",
        "## 3.9 Token 管理 — TokenManager",
        "登录时生成 Token，存储到内存缓存（ShardedMap 32 分片）加 SQLite 持久化。服务端重启时从数据库加载所有 Token 到内存，保证用户不丢失登录态。MessageDispatcher 在分发前自动检查 Token 有效性。",
        "## 3.10 图片存储 — ImageStorage",
        "图片以 BLOB 形式存储到 SQLite，支持缩略图生成。实现 7 天自动过期清理机制，过期图片返回错误码 ERR_IMAGE_EXPIRED（4040）。",
        "## 3.11 文件传输 — FileTransfer",
        "支持大文件分块传输和断点续传。传输流程：发送端发起 FileReq（携带文件名、大小），服务端回复 FileRsp（携带已接收 offset），发送端从 offset 开始以 4KB 为单位逐块发送 FileChunk，每块等待 FileAck 确认。",
    ]),
    ("四、实验过程原始记录（数据，图表，计算等）", [
        "## 4.1 并发架构实测",
        "服务端整体并发架构分为四层：网络 I/O 层（AsioIOServicePool，N 个 io_context + N 线程）→ 业务调度层（LogicSystem + ThreadPool，M 个 Worker 线程）→ 消息分发层（MessageDispatcher，根据 msg_id 分发）→ 处理器层（各 Handler）。数据库连接池维护 8 个 SQLite 连接，通过 RAII 模式自动管理。",
        "## 4.2 消息处理器注册表",
        "共注册 14 个消息处理器：1000（心跳响应）、1001（获取邮箱验证码）、1002（用户注册）、1003（重置密码）、1004（用户登录验证）、1005（聊天会话 Token 鉴权）、1006（文本消息处理与转发）、1008（离线消息分页确认）、1009（图片消息处理）、1011（消息撤回）、1012（消息编辑）、1013（图片下载请求）、2001-2004（文件传输系列）。",
        "## 4.3 安全机制验证",
        "安全措施包括：密码存储采用 SHA-256 加随机盐值哈希，数据库不保存明文；会话鉴权采用 Token 机制，登录后生成，后续操作校验；Token 持久化到内存缓存加 SQLite，重启不丢失；撤回和编辑操作需验证消息所有权加 2 分钟时间窗口；连接安全通过 strand 串行化、CAS 原子标志防并发和 30 秒读超时保证；分片锁（ShardedMap 32 分片）降低锁竞争。",
        "## 4.4 单元测试结果",
        "使用 Google Test 框架进行测试：test_ShardedMap.cpp 测试多分片哈希表并发读写正确性，通过；test_ThreadPool.cpp 测试线程池任务提交与执行，通过；test_ImageStorage.cpp 测试图片存储、缩放、格式转换，通过；stress_image_upload.cpp 进行图片上传压力测试，通过。",
        "## 4.5 构建验证",
        "使用 CMake 3.15+ 构建，依赖 Boost.Asio 1.8x+、Protobuf 3.x、SQLite3、spdlog、OpenSSL。CMake 自动处理 Protobuf 代码生成和跨平台编译器检测（MSVC/GCC/Clang），Debug 模式自动启用 AddressSanitizer。在 Windows 和 Linux 环境下均编译通过。",
        "## 4.6 通信协议格式",
        "二进制帧格式：消息 ID（2 字节，uint16）+ 消息体长度（4 字节，uint32）+ 消息体（Protobuf，变长，最大 1MB）。头部固定 6 字节，小端序。以文本消息为例，Protobuf 定义包含 from_uid、to_uid、content、client_msg_id、timestamp 五个字段。",
    ]),
    ("五、实验结果分析或总结", [
        "通过本次实验，成功实现了基于 Boost.Asio 的高并发即时通讯服务端，主要收获如下：",
        "在异步编程方面，深入理解了 Boost.Asio 的 io_context、strand、异步读写等核心概念，掌握了 Proactor 模式的实践应用。通过 async_accept 非阻塞接受连接，配合 strand 串行化保证了同一 Session 的操作安全。",
        "在并发架构方面，通过 Boss-Worker I/O 线程池加 LogicSystem 业务线程池的双层架构，实现了网络 I/O 与业务逻辑的完全解耦。ShardedMap 分片锁（32 分片）有效降低了锁竞争，连接池的 RAII 管理简化了资源生命周期。",
        "在协议设计方面，自定义二进制帧加 Protobuf 序列化的方案，在保证可读性的同时兼顾了性能。二进制帧头部仅 6 字节，Protobuf 编码体积比 JSON 小 3-10 倍，强类型 schema 在编译期即可检查消息结构。",
        "在安全机制方面，实现了多层安全措施：SHA-256 加盐值密码哈希、Token 鉴权（内存加数据库双存储）、消息所有权验证加时间窗口限制、strand 串行化加 CAS 防并发加读超时检测。",
        "在数据库设计方面，SQLite 连接池配合 WAL 模式，在嵌入式数据库场景下实现了良好的并发读写能力。8 连接池通过 RAII 模式管理，使用方便且不会泄漏。",
        "不足之处在于：当前服务端为单机部署，未实现分布式架构和负载均衡；消息存储仅支持 SQLite，大规模部署需迁移到 MySQL 或 PostgreSQL；日志系统虽集成了 spdlog 但日志分析和告警机制尚不完善。",
    ]),
]


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 报告3：软件功能设计（系统测试 / 功能设计）
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DESIGN_SECTIONS = [
    ("一、实验目的", [
        "1. 掌握即时通讯（IM）系统的功能需求分析方法，学习模块化设计与业务流程设计的实践。",
        "2. 理解 C/S 架构下客户端与服务端的功能划分与协作机制。",
        "3. 掌握通信协议设计、消息路由、离线消息等 IM 核心功能的设计原理。",
        "4. 学习用户认证、文件传输、图片传输等功能的完整业务流程设计。",
        "5. 理解软件安全性设计，包括密码加密、Token 鉴权和消息防篡改。",
    ]),
    ("二、实验原理", [
        "## 2.1 C/S 架构与分层设计",
        "msrChat 采用客户端/服务器（C/S）架构。客户端负责 UI 展示和用户交互，服务端负责业务处理和数据存储。两者通过自定义二进制协议在 TCP 连接上通信。这种架构的优点是职责清晰、便于独立开发和部署。系统整体分为四层：网络传输层（TCP 连接管理）、协议层（二进制帧 + Protobuf 序列化）、业务逻辑层（消息路由、用户认证、文件传输等）和数据持久层（SQLite 数据库）。",
        "## 2.2 即时通讯核心概念",
        "即时通讯系统的核心挑战包括：消息的可靠投递（通过 ACK 确认机制和离线消息队列保证）、实时性（通过长连接和心跳保活维持连接状态）、消息一致性（通过全局递增的 server_msg_id 保证消息有序）、以及用户体验（通过异步发送和状态反馈实现流畅交互）。",
        "## 2.3 用户认证安全原理",
        "用户认证采用两阶段方案。第一阶段为密码验证：客户端发送 UID 和密码，服务端将密码加盐后进行 SHA-256 哈希，与数据库存储的哈希值比对。验证通过后生成随机 Token 返回给客户端，同时在服务端内存和数据库中持久化。第二阶段为会话认证：客户端在建立聊天连接时携带 UID 和 Token，服务端校验 Token 有效性后注册在线会话。这种分离设计使得密码验证和业务会话解耦，Token 持久化保证服务端重启后用户无需重新登录。",
        "## 2.4 消息路由与离线存储原理",
        "消息路由的核心是根据目标用户 UID 查询其在线状态。在线则通过已建立的 TCP 连接直接转发；离线则将消息暂存到数据库的离线消息表中。当用户上线时，服务端分页推送离线消息（每页 50 条），客户端确认收到后请求下一页，直到所有离线消息推送完毕。全局递增的 server_msg_id 由服务端统一管理，确保消息的全局唯一性和有序性。",
        "## 2.5 错误码体系与模块协作设计原则",
        "良好的错误码体系是系统可维护性的基础。本项目采用分层错误码设计：0 表示成功，1000 段表示认证类错误，4000 段表示业务类错误。每个错误码对应明确的语义，客户端可根据错误码向用户展示友好的提示信息。在模块协作方面，采用消息 ID 作为模块间的唯一标识，客户端和服务端通过相同的 Protobuf schema 保证消息结构一致性，实现了模块间的松耦合协作。这种设计使得客户端和服务端可以独立开发和测试，只需保证协议一致即可。",
    ]),
    ("三、实验内容", [
        "## 3.1 系统功能总体设计",
        "msrChat 系统分为五大功能模块：用户认证模块（邮箱注册、用户登录、密码重置、Token 鉴权、验证码）、消息通信模块（文本消息、图片消息、消息撤回、消息编辑、消息 ACK、离线消息）、文件传输模块（分块传输、断点续传、MD5 校验、进度显示）、图片传输模块（图片上传、图片下载、缩略图、本地缓存、全屏查看）、系统保障模块（心跳保活、自动重连、消息持久化、日志系统）。",
        "## 3.2 用户认证模块设计",
        "注册流程采用邮箱验证码加两步注册方案。用户输入邮箱后请求验证码，服务端生成验证码发送到邮箱（有效期 600 秒）；用户输入验证码、用户名和密码后提交注册，服务端验证验证码、检查用户名唯一性，然后将密码进行 SHA-256 加盐值哈希后写入数据库。",
        "登录流程采用两阶段方案。第一阶段密码验证：客户端发送 UID 和密码，服务端验证通过后生成 Token 并返回。第二阶段聊天会话认证：客户端携带 UID 和 Token 连接聊天服务端，服务端校验 Token 后注册在线会话并开始推送离线消息。",
        "密码重置流程与注册类似，用户输入邮箱获取验证码，验证通过后输入新密码，服务端更新数据库中的密码哈希值。",
        "错误码体系包括：ERR_SUCCESS（0，操作成功）、ERR_VERIFY_EXPIRED（1003，验证码过期）、ERR_VERIFY_WRONG（1004，验证码错误）、ERR_USER_EXIST（1005，用户已存在）、ERR_PASSWD_ERR（1006，密码错误）、ERR_USER_NOT_EXIST（1007，用户不存在）、ERR_EMAIL_NOT_MATCH（1008，邮箱不匹配）。",
        "## 3.3 消息通信模块设计",
        "通信协议采用自定义二进制帧格式：消息 ID（2 字节）加消息体长度（4 字节）加 Protobuf 消息体（变长，最大 1MB）。系统定义了 20 余种消息类型，按功能分为心跳（1000）、认证（1001-1005）、聊天（1006-1008）、图片（1009、1010、1013）、撤回编辑（1011-1012、1014-1015）、文件（2001-2004）六组。",
        "文本消息发送流程：用户在 ChatView 输入消息点击发送，ChatController 生成 UUID 标识的 client_msg_id，通过 TcpMgr 发送 Protobuf 消息，同时在本地插入待确认记录并显示发送中状态。服务端收到后生成全局递增的 server_msg_id，持久化到数据库，通过 MessageRouter 路由到目标用户（在线直接转发，离线存入离线表），同时回送 ChatAck 给发送方。",
        "消息撤回功能：仅允许撤回自己发送的消息，撤回时间窗口为发送后 2 分钟内。客户端发送 MSG_CHAT_RECALL（1011），服务端验证时间窗口、消息所有权和撤回状态，通过后在数据库标记撤回，返回 ACK 给请求者，同时发送 MSG_CHAT_RECALL_NOTIFY（1014）通知对方客户端更新 UI。",
        "消息编辑功能：规则与撤回相同（2 分钟窗口加所有权验证）。客户端发送 MSG_CHAT_EDIT（1012），服务端验证通过后更新数据库内容，返回 EditAck 给编辑者，同时发送 MSG_CHAT_EDIT_NOTIFY（1015）通知对方。",
        "离线消息设计：对方不在线时服务端暂存消息，对方上线后分页推送（每页 50 条）。客户端接收后发送 MSG_OFFLINE_ACK 确认页码，服务端收到确认后发送下一页。全部推送完毕后，FlushRecallNotifies() 发送待处理的撤回和编辑通知。",
        "## 3.4 文件传输模块设计",
        "采用分块传输加逐块确认方案。发送端发起 MSG_FILE_REQ（携带 task_id、filename、size、md5），服务端回复 MSG_FILE_RSP（携带 offset，支持断点续传），然后发送端以 4KB 为单位逐块发送 MSG_FILE_CHUNK，每块等待 MSG_FILE_ACK 确认，循环直到传输完成。接收方完成后计算 MD5 并与发送方比对，不一致则标记传输失败。",
        "用户体验方面，提供实时进度条显示（已发送/总大小百分比），右上角浮层面板不影响聊天操作，传输完成后支持另存为操作。",
        "## 3.5 图片传输模块设计",
        "图片发送流程：用户选择图片后，ChatController 读取图片文件，提取元数据（宽度、高度、扩展名、大小、MD5），通过 MSG_CHAT_IMAGE（1009）发送到服务端，UI 端显示 ImageBubble 缩略图预览。",
        "图片接收与缓存：接收方收到图片消息后，ImageDownloadMgr 先检查本地磁盘缓存，再检查内存缓存，均未命中则发起 MSG_IMAGE_DOWNLOAD_REQ（1013）下载请求。采用两级缓存（内存加磁盘）加下载队列加最多 3 次重试的策略。",
        "图片查看器：全屏 ImageViewer 组件，支持双指/滚轮缩放、旋转查看、翻页浏览和另存为本地文件。",
        "图片过期机制：服务端图片 BLOB 存储，7 天自动过期，过期后返回错误码 ERR_IMAGE_EXPIRED（4040），客户端显示图片已过期占位图。",
        "## 3.6 系统保障功能设计",
        "心跳保活：客户端每 15 秒发送 MSG_HELLO（ping），每 5 秒检查是否收到 pong，超过 45 秒未收到则判定连接断开。",
        "自动重连：采用指数退避策略，断开后等待 3 秒重连，失败则翻倍（6 秒、12 秒），最大等待 60 秒持续重连。连接状态机为 Idle → Connecting → Connected → Reconnecting → Stopping。",
        "消息持久化：客户端使用 SQLite messages 表存储消息记录，包含 client_msg_id、server_msg_id、from_uid、to_uid、content、timestamp、status（发送中/已发送/失败）、type（文本/图片/文件）、recalled 和 edited 标记。通过 DbWorker 独立线程异步写入，避免阻塞 UI。",
        "服务端数据库包含 users（用户信息）、messages（聊天消息）、offline_messages（离线消息队列）、tokens（会话 Token）、verify_codes（验证码）、images（图片 BLOB 存储）六张核心表。",
    ]),
    ("四、实验过程原始记录（数据，图表，计算等）", [
        "## 4.1 功能矩阵",
        "系统共实现 11 项核心功能，每项功能均涉及客户端模块、服务端模块和协议消息 ID 的协作。具体为：用户注册（AuthController + LogicSystem，1002）、用户登录（AuthController + TokenManager，1004）、密码重置（AuthController + SQLiteMgr，1003）、聊天登录（AuthController + SessionManager，1005）、文本消息（ChatController + MessageRouter，1006）、图片传输（ImageDownloadMgr + ImageStorage，1009）、文件传输（FileSendMgr/FileRecvMgr + FileTransfer，2001-2004）、消息撤回（ChatController + MessageRouter，1011）、消息编辑（ChatController + MessageRouter，1012）、离线消息（ChatController + MessageRouter，1008）、心跳保活（TcpWorker + CSession，1000）。",
        "## 4.2 安全设计验证",
        "安全措施覆盖六个维度：密码泄露防御（SHA-256 加随机盐值哈希，数据库不存明文）、会话劫持防御（Token 鉴权机制，登录后生成唯一 Token）、消息篡改防御（撤回和编辑需验证所有权加时间窗口）、重放攻击防御（全局递增 server_msg_id 加 client_msg_id 去重）、连接伪造防御（读超时 30 秒加 CAS 原子标志防并发）、数据溢出防御（RingBuffer 最大 4MB 加单包最大 1MB）。",
        "## 4.3 数据库设计记录",
        "服务端数据库 ER 关系：users 与 messages 为一对多关系（发送/接收），users 与 offline_messages 为一对多关系（离线消息队列），users 与 tokens 为一对一关系（会话鉴权），users 与 images 为一对多关系（图片上传）。客户端数据库为单表 messages，按 (from_uid, to_uid) 对存储聊天记录，按 timestamp 排序查询，按 recalled 过滤已撤回消息，通过 DbWorker 异步写入避免阻塞 UI。",
        "## 4.4 UI 交互设计记录",
        "采用双窗口架构。AuthWindow（认证窗口）：无边框 376×540 像素，紫色渐变主题，StackView 管理登录、注册、重置密码三个页面。ChatWindow（聊天窗口）：920×660 像素，包含消息列表 ListView、消息气泡 MessageBubble（己方蓝色右对齐、对方白色左对齐）、图片气泡 ImageBubble（缩略图预览、加载中和失败占位态）、图片查看器 ImageViewer（Loader 延迟加载）、输入区加工具栏、编辑消息对话框 EditMessageDialog 和右键菜单 MessageActionMenu。设计主题色为 #4F46E5（Indigo），背景 #F8F9FE，成功提示 #10B981，错误提示 #EF4444。",
    ]),
    ("五、实验结果分析或总结", [
        "通过本次实验，完成了 msrChat 即时通讯系统的功能设计工作，主要收获如下：",
        "在需求分析方面，成功将 IM 系统拆分为用户认证、消息通信、文件传输、图片传输、系统保障五大模块，每个模块独立设计、松耦合协作。这种模块化设计方法使得三名团队成员可以并行开发不同模块，提高了开发效率。",
        "在协议设计方面，自定义二进制帧加 Protobuf 的组合方案兼顾了性能和可维护性。6 字节头部简洁高效，Protobuf 的强类型 schema 保证了客户端和服务端的消息结构一致性。20 余种消息类型覆盖了 IM 系统的全部核心功能。",
        "在业务流程设计方面，完整设计了注册、登录、消息收发、撤回、编辑、离线消息、文件传输等核心业务流程。特别是两阶段登录、离线消息分页推送、断点续传等设计，体现了对实际使用场景的深入思考。",
        "在安全设计方面，建立了多层安全防御体系：SHA-256 加盐密码哈希防止密码泄露，Token 鉴权防止会话劫持，消息所有权验证加时间窗口防止篡改，全局消息 ID 加 client_msg_id 防止重放攻击，连接超时加 CAS 标志防止伪造。六项安全措施覆盖了常见的安全威胁。",
        "在用户体验设计方面，双窗口架构清晰分离了认证和聊天两个阶段。消息气泡的差异化显示（己方蓝色、对方白色、撤回灰色斜体、编辑标记）、图片的两级缓存和全屏查看器、文件传输的实时进度面板和断点续传等功能设计，都显著提升了用户体验。",
        "不足之处在于：当前设计仅支持单人聊天，群聊功能的协议和 UI 设计尚未涉及；消息搜索功能缺失，用户无法检索历史消息；通知机制（如系统消息推送）的设计还不够完善。这些功能可在后续迭代中逐步补充。",
    ]),
]


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 生成三份报告
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

if __name__ == "__main__":
    print("Generating reports...")

    generate_report(
        "实验报告1-客户端编程实现_新版.docx",
        name="（客户端组）",
        xuehao="2300710329",
        kecheng="软件工程",
        xiangmu="实验一：客户端子系统设计",
        laoshi="唐  敏",
        sections=CLIENT_SECTIONS,
    )

    generate_report(
        "实验报告2-后端编程实现_新版.docx",
        name="（后端组）",
        xuehao="2300710329",
        kecheng="软件工程",
        xiangmu="实验二：服务端子系统设计",
        laoshi="唐  敏",
        sections=BACKEND_SECTIONS,
    )

    generate_report(
        "实验报告3-软件功能设计_新版.docx",
        name="（功能设计组）",
        xuehao="2300710329",
        kecheng="软件工程",
        xiangmu="实验三：系统测试设计",
        laoshi="唐  敏",
        sections=DESIGN_SECTIONS,
    )

    print("Done!")
