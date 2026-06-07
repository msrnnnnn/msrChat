/**
 * @file    main.cpp
 * @brief   客户端应用程序入口文件（QML 版）
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
#include <QGuiApplication>
#include <QIcon>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/image/mainwidow.ico"));

    QString app_path = QCoreApplication::applicationDirPath();
    QString db_path = QDir::toNativeSeparators(app_path + QDir::separator() + "chat_messages.db");
    if (!DbThreadManager::Instance().Init(db_path))
        qWarning() << "Failed to initialize database at:" << db_path;

    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");
    if (!QFile::exists(config_path)) {
        QString current_config = QDir::toNativeSeparators(QDir::currentPath() + QDir::separator() + "config.ini");
        if (QFile::exists(current_config)) config_path = current_config;
    }
    QSettings settings(config_path, QSettings::IniFormat);
    ServerInfo si;
    si.Host = settings.value("ChatServer/host", "127.0.0.1").toString();
    si.Port = settings.value("ChatServer/port", "8080").toString();

    UserMgr::Init();
    TcpMgr::Init();

    QQmlApplicationEngine engine;

    AuthController *authCtrl = new AuthController(&app);
    engine.rootContext()->setContextProperty("authController", authCtrl);

    ChatController *chatCtrl = new ChatController(&app);
    ChatListModel *chatModel = new ChatListModel(&app);
    chatModel->SetCurrentUid(UserMgr::Instance()->GetUid());
    chatCtrl->setChatModel(chatModel);
    chatCtrl->initialize();
    engine.rootContext()->setContextProperty("chatController", chatCtrl);
    engine.rootContext()->setContextProperty("_chatModel", chatModel);

    const QUrl url(QStringLiteral("qrc:/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    QTimer::singleShot(0, [si]() { TcpMgr::Instance()->slotTcpConnect(si); });

    int exit_code = app.exec();
    TcpMgr::Destroy();
    UserMgr::Destroy();
    DbThreadManager::Instance().cleanup();
    return exit_code;
}
