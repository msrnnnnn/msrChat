/**
 * @file Utils.cpp
 * @brief 通用工具类实现
 */

#include "Utils.h"
#include <QCryptographicHash>
#include <QStyle>

/**
 * @brief 获取密码盐值（静态常量，编译期确定）
 * @return 盐值字符串引用
 */
const QString &Utils::salt()
{
    static const QString SALT = QStringLiteral("MsrChat_v1_Salt_2024");
    return SALT;
}

/**
 * @brief 对密码进行加盐 SHA-256 哈希
 * @param input 原始密码
 * @return 哈希结果（格式：盐值 + 64位十六进制哈希）
 */
QString Utils::hashPassword(const QString &input)
{
    // 加盐后做 SHA-256 哈希，结果格式：盐值 + 十六进制哈希
    QString salted = input + salt();
    QByteArray data = QCryptographicHash::hash(salted.toUtf8(), QCryptographicHash::Sha256);
    return salt() + data.toHex();
}

/**
 * @brief 强制刷新控件样式
 * @param w 目标控件
 * @details 先 unpolish 再 polish 触发 QStyle 重新计算，常用于动态切换样式表后刷新界面。
 */
void Utils::repolish(QWidget *w)
{
    if (w)
    {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}
