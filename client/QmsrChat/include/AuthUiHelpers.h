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
inline QString ValidateUsername(const QString &username)
{
    return username.trimmed().isEmpty() ? QStringLiteral("用户名不能为空") : QString();
}

inline QString ValidateEmail(const QString &email)
{
    static const QRegularExpression kEmailRegex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    return kEmailRegex.match(email.trimmed()).hasMatch() ? QString() : QStringLiteral("邮箱地址不正确");
}

inline QString ValidatePassword(const QString &password)
{
    if (password.length() < 6 || password.length() > 15)
    {
        return QStringLiteral("密码长度应为6~15");
    }

    static const QRegularExpression kPasswordRegex(QStringLiteral("^[a-zA-Z0-9!@#$%^&*]{6,15}$"));
    return kPasswordRegex.match(password).hasMatch() ? QString() : QStringLiteral("不能包含非法字符");
}

inline QString ValidateConfirmPassword(const QString &password, const QString &confirm)
{
    if (confirm.isEmpty())
    {
        return QStringLiteral("确认密码不能为空");
    }
    return confirm == password ? QString() : QStringLiteral("密码和确认密码不匹配");
}

inline QString ValidateVerifyCode(const QString &verifyCode)
{
    return verifyCode.trimmed().isEmpty() ? QStringLiteral("验证码不能为空") : QString();
}

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

template <typename ErrorMap>
inline void AddTipError(ErrorMap &errors, TipErr key, const QString &message, QLabel *label)
{
    errors[key] = message;
    ShowTip(label, message, false);
}

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
