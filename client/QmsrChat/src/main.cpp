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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString app_path = QCoreApplication::applicationDirPath();
    QString db_path = QDir::toNativeSeparators(app_path + QDir::separator() + "chat_messages.db");

    if (!DbThreadPool::Instance().Init(db_path))
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
    QString chat_host = settings.value("ChatServer/host", "127.0.0.1").toString();
    QString chat_port = settings.value("ChatServer/port", "8080").toString();

    qDebug() << "Config Path:" << config_path;
    qDebug() << "ChatServer:" << chat_host << ":" << chat_port;

    ServerInfo si;
    si.Host = chat_host;
    si.Port = chat_port;
    si.Token = "";
    si.Uid = 0;

    qDebug() << "Initializing UserMgr...";
    UserMgr::Init();

    qDebug() << "Initiating TCP connection to ChatServer...";
    TcpMgr::Init();
    TcpMgr::Instance()->slot_tcp_connect(si);

    MainWindow w;
    w.show();
    int exit_code = a.exec();

    TcpMgr::Destroy();
    UserMgr::Destroy();
    DbThreadPool::Destroy();
    return exit_code;
}
