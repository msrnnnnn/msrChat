/**
 * @file    main.cpp
 * @brief   客户端应用程序入口文件（QML 版 — 双窗口架构）
 * @details AuthWindow（登录/注册/重置）和 ChatWindow（聊天）为独立的 ApplicationWindow，
 *          由 main.cpp 管理其生命周期：登录成功后关 auth 开 chat；token 失效关 chat 开 auth。
 */
#include "AuthController.h"
#include "ChatController.h"
#include "ChatListModel.h"
#include "DbWorker.h"
#include "Global.h"
#include "TcpMgr.h"
#include "UserMgr.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTimer>

// 全局窗口指针 — 同一时间只有一个窗口存活
static QObject *g_authWindow  = nullptr;
static QObject *g_chatWindow  = nullptr;

// 前向声明辅助函数
static void createAuthWindow(QQmlApplicationEngine &engine);
static void createChatWindow(QQmlApplicationEngine &engine);

static void createAuthWindow(QQmlApplicationEngine &engine)
{
    if (g_authWindow) return;

    QQmlComponent comp(&engine, QUrl(QStringLiteral("qrc:/AuthWindow.qml")));
    if (comp.isError()) {
        qWarning() << "[main] AuthWindow compile errors:";
        for (const auto &e : comp.errors()) qWarning() << "  " << e.toString();
        return;
    }
    g_authWindow = comp.create();
    if (!g_authWindow) {
        qWarning() << "[main] Failed to create AuthWindow";
        return;
    }
    g_authWindow->setProperty("visible", true);
    qDebug() << "[main] AuthWindow created and shown";
}

static void createChatWindow(QQmlApplicationEngine &engine)
{
    if (g_chatWindow) return;

    QQmlComponent comp(&engine, QUrl(QStringLiteral("qrc:/ChatWindow.qml")));
    if (comp.isError()) {
        qWarning() << "[main] ChatWindow compile errors:";
        for (const auto &e : comp.errors()) qWarning() << "  " << e.toString();
        return;
    }
    g_chatWindow = comp.create();
    if (!g_chatWindow) {
        qWarning() << "[main] Failed to create ChatWindow";
        return;
    }
    g_chatWindow->setProperty("visible", true);
    qDebug() << "[main] ChatWindow created and shown";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/image/mainwidow.ico"));

    // ===== 数据库初始化 =====
    QString app_path = QCoreApplication::applicationDirPath();
    QString db_path = QDir::toNativeSeparators(app_path + QDir::separator() + "chat_messages.db");
    if (!DbThreadManager::Instance().Init(db_path))
        qWarning() << "Failed to initialize database at:" << db_path;

    // ===== 读取配置 =====
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");
    if (!QFile::exists(config_path)) {
        QString current_config = QDir::toNativeSeparators(QDir::currentPath() + QDir::separator() + "config.ini");
        if (QFile::exists(current_config)) config_path = current_config;
    }
    QSettings settings(config_path, QSettings::IniFormat);
    ServerInfo si;
    si.Host = settings.value("ChatServer/host", "127.0.0.1").toString();
    si.Port = settings.value("ChatServer/port", "8080").toString();

    // ===== 初始化全局管理器 =====
    UserMgr::Init();
    TcpMgr::Init();

    // ===== QML 引擎 + 上下文注入 =====
    QQmlApplicationEngine engine;

    AuthController *authCtrl = new AuthController(&app);
    engine.rootContext()->setContextProperty("authController", authCtrl);

    ChatController *chatCtrl = new ChatController(&app);
    ChatListModel  *chatModel = new ChatListModel(&app);
    chatModel->SetCurrentUid(UserMgr::Instance()->GetUid());
    chatCtrl->setChatModel(chatModel);
    // 注意：不在这里调用 chatCtrl->initialize()，由 ChatWindow.Component.onCompleted 调用
    engine.rootContext()->setContextProperty("chatController", chatCtrl);
    engine.rootContext()->setContextProperty("_chatModel", chatModel);

    // ===== 窗口生命周期管理 =====

    // 登录成功 → 关闭 AuthWindow，打开 ChatWindow
    QObject::connect(authCtrl, &AuthController::chatLoginSuccess, [&engine]() {
        qDebug() << "[main] chatLoginSuccess — switching to ChatWindow";
        if (g_authWindow) {
            g_authWindow->setProperty("visible", false);
            g_authWindow->deleteLater();
            g_authWindow = nullptr;
        }
        createChatWindow(engine);
    });

    // Token 失效 → 关闭 ChatWindow，重新打开 AuthWindow
    QObject::connect(authCtrl, &AuthController::tokenInvalid, [&engine](const QString &msg) {
        qDebug() << "[main] tokenInvalid:" << msg << "— switching to AuthWindow";
        if (g_chatWindow) {
            g_chatWindow->setProperty("visible", false);
            g_chatWindow->deleteLater();
            g_chatWindow = nullptr;
        }
        createAuthWindow(engine);
    });

    // ===== 启动：显示 AuthWindow =====
    createAuthWindow(engine);

    // ===== 延迟发起 TCP 连接 =====
    QTimer::singleShot(0, [si]() { TcpMgr::Instance()->slotTcpConnect(si); });

    // ===== 运行事件循环 =====
    int exit_code = app.exec();

    // ===== 清理 =====
    TcpMgr::Destroy();
    UserMgr::Destroy();
    DbThreadManager::Instance().cleanup();
    return exit_code;
}
