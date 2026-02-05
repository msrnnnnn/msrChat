/**
 * @file global.cpp
 * @brief 全局定义实现文件
 * @details 包含全局变量的定义和初始化。
 * @author msr
 */

#include "global.h"
#include <QStyle>

/**
 * @brief 刷新 QSS 样式的 Lambda 实现
 * @param w 需要刷新的 QWidget 指针
 */
std::function<void(QWidget *)> repolish = [](QWidget *w)
{
    if (w)
    {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
};

/**
 * @brief 网关 URL 前缀定义
 * @details 默认为空字符串，将在运行时从配置加载。
 */
QString gate_url_prefix = "";
