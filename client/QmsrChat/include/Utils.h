#pragma once
/**
 * @file Utils.h
 * @brief 通用工具类
 * @details 提供密码哈希、样式刷新等工具函数。
 */

#ifndef UTILS_H
#define UTILS_H

#include <QString>

class Utils
{
public:
    static QString hashPassword(const QString &input);
    static QString hmacSha256(const QString &key, const QString &message);

private:
    Utils() = default;
    static const QString &salt();
};

#endif
