/**
 * @file    main.cpp
 * @brief   客户端应用程序入口文件
 * @details 负责初始化 Qt 应用程序，加载 QSS 样式表，读取配置文件，并显示主窗口。
 */

#include "global.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

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
    QString app_path = QCoreApplication::applicationDirPath();

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
    QString gate_host = settings.value("GateServer/host", "192.168.226.129").toString();
    QString gate_port = settings.value("GateServer/port", "8080").toString();

    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    qDebug() << "Config Path:" << config_path;
    qDebug() << "Gate Server:" << gate_url_prefix;

    MainWindow w;
    w.show();
    return a.exec();
}
