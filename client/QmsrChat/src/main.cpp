/**
 * @file    main.cpp
 * @brief   客户端应用程序入口文件
 * @details 负责初始化 Qt 应用程序，加载 QSS 样式表，读取配置文件，并显示主窗口。
 */

#include "DbWorker.h"
#include "Global.h"
#include "MainWindow.h"
#include "TcpMgr.h"
#include "UserMgr.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    // 强制使用 Fusion 样式，保证跨平台界面一致性
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication a(argc, argv);

    // 初始化 SQLite 数据库（聊天消息持久化存储）
    QString app_path = QCoreApplication::applicationDirPath();
    QString db_path = QDir::toNativeSeparators(app_path + QDir::separator() + "chat_messages.db");

    if (!DbThreadManager::Instance().Init(db_path))
    {
        qWarning() << "Failed to initialize database at:" << db_path;
    }
    else
    {
        qDebug() << "Database initialized successfully at:" << db_path;
    }

    // 加载 QSS 样式表
    QFile qss(":/style/stylesheet.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream stream(&qss);
        QString styleSheet = stream.readAll();
        a.setStyleSheet(styleSheet);
        qss.close();
        qDebug() << "StyleSheet loaded successfully.";
    }
    else
    {
        qDebug() << "Warning: Failed to load stylesheet:" << qss.errorString();
    }

    // 加载配置文件逻辑

    // 优先读取本地配置文件
    // 配置文件查找策略：优先可执行文件目录，其次当前工作目录
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");

    if (!QFile::exists(config_path))
    {
        QString current_config = QDir::toNativeSeparators(QDir::currentPath() + QDir::separator() + "config.ini");
        if (QFile::exists(current_config))
        {
            config_path = current_config;
            qDebug() << "Redirecting to current path config:" << config_path;
        }
        else
        {
            qDebug() << "Warning: config.ini not found in app dir or current path.";
        }
    }

    QSettings settings(config_path, QSettings::IniFormat);
    // 读取 ChatServer 配置段，默认值用于本地开发
    QString chat_host = settings.value("ChatServer/host", "127.0.0.1").toString();
    QString chat_port = settings.value("ChatServer/port", "8080").toString();

    qDebug() << "Config Path:" << config_path;
    qDebug() << "ChatServer:" << chat_host << ":" << chat_port;

    ServerInfo si;
    si.Host = chat_host;
    si.Port = chat_port;

    qDebug() << "Initializing UserMgr...";
    UserMgr::Init();

    qDebug() << "Initiating TCP connection to ChatServer...";
    TcpMgr::Init();

    MainWindow w;
    w.show();

    // 延迟到主事件循环启动后，工作线程的 slot_init 也已完成，再发起连接
    // TcpMgr::Init() 会创建 Worker（QThread），真正的 TCP 连接需延迟到事件循环启动后
    // 通过 QTimer::singleShot(0, ...) 确保 Worker 的 event loop 已就绪
    QTimer::singleShot(0, [si]() {
        TcpMgr::Instance()->slot_tcp_connect(si);
    });

    int exit_code = a.exec();

    // 事件循环退出后按依赖顺序销毁（先销毁网络连接，再用户管理器，最后数据库）
    TcpMgr::Destroy();
    UserMgr::Destroy();
    DbThreadManager::Instance().cleanup();
    return exit_code;
}
