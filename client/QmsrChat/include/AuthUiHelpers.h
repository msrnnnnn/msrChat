/**
 * @file AuthUiHelpers.h
 * @brief 认证界面辅助函数集合
 * @details 提供用户名、邮箱、密码等表单字段的验证逻辑，以及界面提示、密码显隐切换等通用 UI 工具函数。
 */
#ifndef AUTHUIHELPERS_H
#define AUTHUIHELPERS_H

#include "ClickedLabel.h"
#include "Utils.h"
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QRegularExpression>

namespace AuthUiHelpers
{
/**
 * @brief 验证用户名是否为空（去除首尾空白后判断）
 * @param username 原始用户名
 * @return 验证通过返回空字符串，失败返回错误提示
 */
inline QString ValidateUsername(const QString &username)
{
    return username.trimmed().isEmpty() ? QStringLiteral("用户名不能为空") : QString();
}

/**
 * @brief 验证邮箱格式是否正确
 * @param email 原始邮箱地址
 * @return 验证通过返回空字符串，失败返回"邮箱地址不正确"
 */
inline QString ValidateEmail(const QString &email)
{
    static const QRegularExpression kEmailRegex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    return kEmailRegex.match(email.trimmed()).hasMatch() ? QString() : QStringLiteral("邮箱地址不正确");
}

/**
 * @brief 验证密码长度和字符合法性
 * @details 要求长度 6~15，仅允许字母、数字及 !@#$%^&* 字符
 * @param password 原始密码
 * @return 验证通过返回空字符串，失败返回错误提示
 */
inline QString ValidatePassword(const QString &password)
{
    if (password.length() < 6 || password.length() > 15)
    {
        return QStringLiteral("密码长度应为6~15");
    }

    static const QRegularExpression kPasswordRegex(QStringLiteral("^[a-zA-Z0-9!@#$%^&*]{6,15}$"));
    return kPasswordRegex.match(password).hasMatch() ? QString() : QStringLiteral("不能包含非法字符");
}

/**
 * @brief 验证确认密码是否与密码一致
 * @param password 原始密码
 * @param confirm 确认密码
 * @return 验证通过返回空字符串，失败返回错误提示
 */
inline QString ValidateConfirmPassword(const QString &password, const QString &confirm)
{
    if (confirm.isEmpty())
    {
        return QStringLiteral("确认密码不能为空");
    }
    return confirm == password ? QString() : QStringLiteral("密码和确认密码不匹配");
}

/**
 * @brief 验证验证码是否为空
 * @param verifyCode 验证码
 * @return 验证通过返回空字符串，失败返回"验证码不能为空"
 */
inline QString ValidateVerifyCode(const QString &verifyCode)
{
    return verifyCode.trimmed().isEmpty() ? QStringLiteral("验证码不能为空") : QString();
}

/**
 * @brief 在标签上显示提示信息，根据校验结果切换 normal/error 样式状态
 * @param label 目标标签控件
 * @param text 提示文本
 * @param isCorrect 校验是否通过
 */
inline void ShowTip(QLabel *label, const QString &text, bool isCorrect)
{
    if (!label)
    {
        return;
    }

    label->setProperty("state", isCorrect ? "normal" : "error");
    label->setText(text);
    Utils::repolish(label);
}

/**
 * @brief 将密码显隐切换标签与密码输入框绑定，点击时切换 echoMode 和显示文本
 * @param toggle 可点击的状态切换标签
 * @param edit 密码输入框
 */
inline void BindPasswordToggle(ClickedLabel *toggle, QLineEdit *edit)
{
    if (!toggle || !edit)
    {
        return;
    }

    toggle->setCursor(Qt::PointingHandCursor);
    toggle->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    toggle->setText(QStringLiteral("显示"));
    edit->setEchoMode(QLineEdit::Password);

    QObject::connect(
        toggle, &ClickedLabel::clicked, edit,
        [toggle, edit]()
        {
            const auto state = toggle->GetCurState();
            const bool masked = state == ClickLbState::Normal;
            edit->setEchoMode(masked ? QLineEdit::Password : QLineEdit::Normal);
            toggle->setText(masked ? QStringLiteral("显示") : QStringLiteral("隐藏"));
        });
}

/**
 * @brief 向错误映射表中添加一条错误，并更新对应标签为 error 状态
 * @param errors 错误映射表（例如 QMap<TipErr, QString>）
 * @param key 错误枚举键
 * @param message 错误提示文本
 * @param label 对应标签控件
 */
template <typename ErrorMap>
inline void AddTipError(ErrorMap &errors, TipErr key, const QString &message, QLabel *label)
{
    errors[key] = message;
    ShowTip(label, message, false);
}

/**
 * @brief 从错误映射表中移除指定错误，若映射表为空则恢复标签为 normal 状态
 * @param errors 错误映射表
 * @param key 错误枚举键
 * @param label 对应标签控件
 */
template <typename ErrorMap>
inline void RemoveTipError(ErrorMap &errors, TipErr key, QLabel *label)
{
    errors.remove(key);
    if (errors.empty())
    {
        ShowTip(label, QString(), true);
        return;
    }

    ShowTip(label, errors.first(), false);
}

/**
 * @brief 根据验证结果决定去添加或移除错误提示
 * @param errors 错误映射表
 * @param key 错误枚举键
 * @param message 验证结果消息（空串表示通过）
 * @param label 对应标签控件
 * @return true 表示验证通过，false 表示验证失败
 */
template <typename ErrorMap>
inline bool ApplyValidationResult(ErrorMap &errors, TipErr key, const QString &message, QLabel *label)
{
    if (!message.isEmpty())
    {
        AddTipError(errors, key, message, label);
        return false;
    }

    RemoveTipError(errors, key, label);
    return true;
}
} // namespace AuthUiHelpers

#endif // AUTHUIHELPERS_H
