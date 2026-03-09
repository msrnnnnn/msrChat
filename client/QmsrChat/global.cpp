/**
 * @file global.cpp
 * @brief 全局定义实现文件
 * @details 包含全局变量的定义和初始化。
 */

#include "global.h"
#include <QStyle>

// 刷新 QSS 样式的实现
std::function<void(QWidget *)> repolish = [](QWidget *w)
{
    if (w)
    {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
};

QString gate_url_prefix = "";

// 简单的异或加密实现
std::function<QString(QString)> xorString = [](QString input)
{
    QByteArray data = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return data.toHex();
};
