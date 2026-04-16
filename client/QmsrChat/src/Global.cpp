/**
 * @file global.cpp
 * @brief 全局定义实现文件
 * @details 包含全局变量的定义和初始化，委托给 Utils 类实现。
 */

#include "Global.h"
#include "Utils.h"

std::function<QString(QString)> hashPassword = [](QString input) { return Utils::hashPassword(input); };

std::function<void(QWidget *)> repolish = [](QWidget *w) { Utils::repolish(w); };
