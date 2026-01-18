#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //加载qss文件
    QFile qss(":/style/stylesheet.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&qss);
        QString styleSheet = stream.readAll();
        a.setStyleSheet(styleSheet);
        qss.close();
    }else {
        // 若打开失败，输出错误提示（方便排查问题）
        qDebug() << "QSS文件打开失败：" << qss.errorString();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
