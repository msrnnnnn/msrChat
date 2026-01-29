#include "mainwindow.h"
#include "global.h"      // 引入全局变量
#include <QApplication>
#include <QFile>
#include <QSettings>     // 引入设置类
#include <QDir>          // 引入目录类
#include <QDebug>        // [Fix] 必须引入，否则 qDebug() 会报错
#include <QTextStream>   // [Fix] 必须引入，否则 QTextStream 会报错

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // [UI Styling] 加载 QSS 样式表
    // 确保你的 resources.qrc 里确实有这个路径，否则会打印警告
    QFile qss(":/style/stylesheet.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&qss);
        QString styleSheet = stream.readAll();
        a.setStyleSheet(styleSheet);
        qss.close();
        qDebug() << "StyleSheet loaded successfully.";
    } else {
        qDebug() << "Warning: Failed to load stylesheet:" << qss.errorString();
    }

    // [Architect Note] 加载配置文件逻辑
    // 1. 获取可执行文件所在目录
    QString app_path = QCoreApplication::applicationDirPath();

    // 2. 拼接完整路径 (自动处理分隔符)
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");

    // 3. 读取配置 (提供默认值防止崩溃)
    QSettings settings(config_path, QSettings::IniFormat);
    QString gate_host = settings.value("GateServer/host", "127.0.0.1").toString();
    QString gate_port = settings.value("GateServer/port", "8080").toString();

    // 4. 组装全局 URL
    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    qDebug() << "Config Path:" << config_path;
    qDebug() << "Gate Server:" << gate_url_prefix;

    MainWindow w;
    w.show();
    return a.exec();
}
