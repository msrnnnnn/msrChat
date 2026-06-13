/**
 * @file Utils.cpp
 * @brief 通用工具类实现
 */

#include "Utils.h"
#include <QCryptographicHash>

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
    QString salted = input + salt();
    QByteArray data = QCryptographicHash::hash(salted.toUtf8(), QCryptographicHash::Sha256);
    return salt() + data.toHex();
}

QString Utils::hmacSha256(const QString &key, const QString &message)
{
    const int blockSize = 64;
    QByteArray keyBytes = key.toUtf8();
    QByteArray msgBytes = message.toUtf8();

    if (keyBytes.size() > blockSize)
        keyBytes = QCryptographicHash::hash(keyBytes, QCryptographicHash::Sha256);

    QByteArray paddedKey(blockSize, 0x00);
    paddedKey.replace(0, keyBytes.size(), keyBytes);

    QByteArray ipad(blockSize, 0x36);
    QByteArray opad(blockSize, 0x5c);

    for (int i = 0; i < blockSize; ++i)
    {
        ipad[i] = ipad[i] ^ paddedKey[i];
        opad[i] = opad[i] ^ paddedKey[i];
    }

    QByteArray inner = QCryptographicHash::hash(ipad + msgBytes, QCryptographicHash::Sha256);
    QByteArray result = QCryptographicHash::hash(opad + inner, QCryptographicHash::Sha256);

    return result.toHex();
}
