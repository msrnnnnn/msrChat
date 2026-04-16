/**
 * @file Utils.h
 * @brief 通用工具类
 * @details 提供密码哈希、样式刷新等工具函数。
 */

#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QWidget>

class Utils
{
public:
    Utils(const Utils &) = delete;
    Utils &operator=(const Utils &) = delete;

    static QString hashPassword(const QString &input);
    static void repolish(QWidget *w);

    static Utils &instance()
    {
        static Utils inst;
        return inst;
    }

private:
    Utils() = default;
    ~Utils() = default;

    static const QString &salt();
};

#endif
