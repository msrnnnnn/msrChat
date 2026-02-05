/**
 * @file    main.cpp
 * @brief   客户端应用程序入口文件
 * @author  msr
 *
 * @details 负责初始化 Qt 应用程序，加载 QSS 样式表，读取配置文件，并显示主窗口。
 */

#include "global.h" // 引入全局变量
#include "mainwindow.h"
#include <QApplication>
#include <QDebug> // [Fix] 必须引入，否则 qDebug() 会报错
#include <QDir>   // 引入目录类
#include <QFile>
#include <QSettings>   // 引入设置类
#include <QTextStream> // [Fix] 必须引入，否则 QTextStream 会报错

/**
 * @brief   主函数
 * @param   argc 参数个数
 * @param   argv 参数列表
 * @return  应用程序退出码
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // [UI Styling] 加载 QSS 样式表
    // 确保你的 resources.qrc 里确实有这个路径，否则会打印警告
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

    // [Architect Note] 加载配置文件逻辑
    // 1. 获取可执行文件所在目录
    QString app_path = QCoreApplication::applicationDirPath();

    // 2. 拼接完整路径 (自动处理分隔符)
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");

    // [Dev Fix] 如果运行目录下没有配置文件，尝试读取源码目录下的配置
    // 解决 Shadow Build 导致修改源码目录下的 config.ini 不生效的问题
    if (!QFile::exists(config_path))
    {
        // [Fix] 修正硬编码路径为当前项目实际路径
        QString source_config = "e:/Study/Project/Chat/msrchat/client/QmsrChat/config.ini";
        if (QFile::exists(source_config))
        {
            config_path = source_config;
            qDebug() << "Redirecting to source config:" << config_path;
        }
    }

    // 3. 读取配置 (提供默认值防止崩溃)
    QSettings settings(config_path, QSettings::IniFormat);
    QString gate_host = settings.value("GateServer/host", "192.168.226.129").toString();
    QString gate_port = settings.value("GateServer/port", "8080").toString();

    // 4. 组装全局 URL
    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    qDebug() << "Config Path:" << config_path;
    qDebug() << "Gate Server:" << gate_url_prefix;

    MainWindow w;
    w.show();
    return a.exec();
}
