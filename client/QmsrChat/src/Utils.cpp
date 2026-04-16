/**
 * @file Utils.cpp
 * @brief 通用工具类实现
 */

#include "Utils.h"
#include <QCryptographicHash>
#include <QStyle>

const QString &Utils::salt()
{
    static const QString SALT = QStringLiteral("MsrChat_v1_Salt_2024");
    return SALT;
}

QString Utils::hashPassword(const QString &input)
{
    QString salted = input + salt();
    QByteArray data = QCryptographicHash::hash(salted.toUtf8(), QCryptographicHash::Sha256);
    return salt() + data.toHex();
}

void Utils::repolish(QWidget *w)
{
    if (w)
    {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}
