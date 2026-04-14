/**
 * @file global.cpp
 * @brief 全局定义实现文件
 * @details 包含全局变量的定义和初始化。
 */

#include "global.h"
#include <QCryptographicHash>
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

// SHA256 密码哈希
std::function<QString(QString)> xorString = [](QString input)
{
    QByteArray data = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return data.toHex();
};
