#include "AuthController.h"
#include "Global.h"
#include "TcpMgr.h"
#include "UserMgr.h"
#include "Utils.h"
#include <QDebug>
#include <QRegularExpression>

AuthController::AuthController(QObject *parent) : QObject(parent) { ConnectTcpSignals(); }
AuthController::~AuthController() = default;

void AuthController::ConnectTcpSignals()
{
    connect(TcpMgr::Instance(), &TcpMgr::sigLoginRsp, this, &AuthController::slotLoginRsp);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatLoginRsp, this, &AuthController::slotChatLoginRsp);
    connect(TcpMgr::Instance(), &TcpMgr::sigRegisterRsp, this, &AuthController::slotRegisterRsp);
    connect(TcpMgr::Instance(), &TcpMgr::sigVerifyCodeRsp, this, &AuthController::slotVerifyCodeRsp);
    connect(TcpMgr::Instance(), &TcpMgr::sigResetPwdRsp, this, &AuthController::slotResetPwdRsp);
}

QString AuthController::validateUsername(const QString &u) const { return u.trimmed().isEmpty() ? QStringLiteral("用户名不能为空") : QString(); }
QString AuthController::validatePassword(const QString &p) const {
    if (p.length() < 6 || p.length() > 15) return QStringLiteral("密码长度应为6~15");
    static const QRegularExpression re(QStringLiteral("^[a-zA-Z0-9!@#$%^&*]{6,15}$"));
    return re.match(p).hasMatch() ? QString() : QStringLiteral("不能包含非法字符");
}
QString AuthController::validateEmail(const QString &e) const {
    static const QRegularExpression re(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    return re.match(e.trimmed()).hasMatch() ? QString() : QStringLiteral("邮箱地址不正确");
}
QString AuthController::validateConfirmPassword(const QString &p, const QString &c) const {
    if (c.isEmpty()) return QStringLiteral("确认密码不能为空");
    return c == p ? QString() : QStringLiteral("密码和确认密码不匹配");
}
QString AuthController::validateVerifyCode(const QString &c) const { return c.trimmed().isEmpty() ? QStringLiteral("验证码不能为空") : QString(); }

void AuthController::login(const QString &username, const QString &password) {
    QString err = validateUsername(username); if (!err.isEmpty()) { emit loginResult(false, err); return; }
    err = validatePassword(password); if (!err.isEmpty()) { emit loginResult(false, err); return; }
    _is_logging_in = true; emit sigIsLoggingInChanged();
    LoginReqStruct req; req.user = username; req.passwd = Utils::hashPassword(password);
    TcpMgr::Instance()->slot_send_login_req(req);
}

void AuthController::slotLoginRsp(const LoginRspStruct &rsp) {
    _is_logging_in = false; emit sigIsLoggingInChanged();
    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS)) {
        QString errStr;
        switch (static_cast<ERRORCODES>(rsp.error)) {
            case ERRORCODES::PasswdErr: errStr = tr("密码错误"); break;
            case ERRORCODES::UserNotExist: errStr = tr("用户不存在"); break;
            default: errStr = tr("登录失败"); break;
        }
        emit loginResult(false, errStr); return;
    }
    _uid = rsp.uid; _token = rsp.token;
    UserMgr::Instance()->SetUid(_uid); UserMgr::Instance()->SetToken(_token);
    emit loginResult(true, tr("登录成功，正在连接聊天服务..."));
    _msg_buffer_connection = connect(TcpMgr::Instance(), &TcpMgr::sigChatTextMsg, this,
        [this](const ChatTextMsgStruct &msg) { _buffered_messages.append(msg); }, Qt::QueuedConnection);
    ChatLoginReqStruct chatReq; chatReq.uid = _uid; chatReq.token = _token;
    TcpMgr::Instance()->slot_send_chat_login_req(chatReq);
}

void AuthController::slotChatLoginRsp(const ChatLoginRspStruct &rsp) {
    if (rsp.error != 0) {
        _uid = 0; _token.clear(); UserMgr::Instance()->SetToken("");
        emit tokenInvalid(rsp.message.isEmpty() ? tr("聊天登录失败") : rsp.message); return;
    }
    emit chatLoginSuccess();
}

QVector<ChatTextMsgStruct> AuthController::TakeBufferedMessages() {
    disconnect(_msg_buffer_connection);
    QVector<ChatTextMsgStruct> msgs = _buffered_messages; _buffered_messages.clear(); return msgs;
}

void AuthController::sendRegisterVerifyCode(const QString &email) {
    QString err = validateEmail(email); if (!err.isEmpty()) { emit verifyCodeResult(false, err); return; }
    VerifyCodeReqStruct req; req.email = email.trimmed();
    TcpMgr::Instance()->slot_send_verify_code_req(req);
}

void AuthController::registerUser(const QString &username, const QString &email, const QString &password, const QString &confirmPassword, const QString &verifyCode) {
    QString err;
    err = validateUsername(username); if (!err.isEmpty()) { emit registerResult(false, err); return; }
    err = validateEmail(email); if (!err.isEmpty()) { emit registerResult(false, err); return; }
    err = validatePassword(password); if (!err.isEmpty()) { emit registerResult(false, err); return; }
    err = validateConfirmPassword(password, confirmPassword); if (!err.isEmpty()) { emit registerResult(false, err); return; }
    err = validateVerifyCode(verifyCode); if (!err.isEmpty()) { emit registerResult(false, err); return; }
    RegisterReqStruct req; req.user = username; req.email = email.trimmed();
    req.passwd = Utils::hashPassword(password); req.verifycode = verifyCode.trimmed();
    TcpMgr::Instance()->slot_send_register_req(req);
}

void AuthController::slotVerifyCodeRsp(const VerifyCodeRspStruct &rsp) {
    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS)) {
        QString errStr;
        switch (static_cast<ERRORCODES>(rsp.error)) {
            case ERRORCODES::EmailNotMatch: errStr = tr("邮箱格式不正确"); break;
            default: errStr = tr("获取验证码失败 (%1)").arg(rsp.error); break;
        }
        emit verifyCodeResult(false, errStr); return;
    }
#ifndef NDEBUG
    emit verifyCodeResult(true, tr("验证码: %1").arg(rsp.code));
#else
    emit verifyCodeResult(true, tr("验证码已生成，请输入"));
#endif
}

void AuthController::slotRegisterRsp(const RegisterRspStruct &rsp) {
    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS)) {
        QString errStr;
        switch (static_cast<ERRORCODES>(rsp.error)) {
            case ERRORCODES::VerifyCodeExpired: errStr = tr("验证码已过期，请重新获取"); break;
            case ERRORCODES::VerifyCodeErr:     errStr = tr("验证码错误"); break;
            case ERRORCODES::UserExist:         errStr = tr("用户名已存在"); break;
            default: errStr = tr("注册失败 (%1)").arg(rsp.error); break;
        }
        emit registerResult(false, errStr); return;
    }
    emit registerResult(true, tr("注册成功！"));
}

void AuthController::resetPassword(const QString &username, const QString &email, const QString &newPassword, const QString &verifyCode) {
    QString err;
    err = validateUsername(username); if (!err.isEmpty()) { emit resetPasswordResult(false, err); return; }
    err = validateEmail(email); if (!err.isEmpty()) { emit resetPasswordResult(false, err); return; }
    err = validatePassword(newPassword); if (!err.isEmpty()) { emit resetPasswordResult(false, err); return; }
    err = validateVerifyCode(verifyCode); if (!err.isEmpty()) { emit resetPasswordResult(false, err); return; }
    ResetPwdReqStruct req; req.user = username; req.email = email.trimmed();
    req.passwd = Utils::hashPassword(newPassword); req.verifycode = verifyCode.trimmed();
    TcpMgr::Instance()->slot_send_reset_pwd_req(req);
}

void AuthController::slotResetPwdRsp(const ResetPwdRspStruct &rsp) {
    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS)) {
        QString errStr;
        switch (static_cast<ERRORCODES>(rsp.error)) {
            case ERRORCODES::VerifyCodeExpired: errStr = tr("验证码已过期，请重新获取"); break;
            case ERRORCODES::VerifyCodeErr:     errStr = tr("验证码错误"); break;
            case ERRORCODES::UserNotExist:      errStr = tr("用户不存在"); break;
            case ERRORCODES::EmailNotMatch:     errStr = tr("邮箱与注册邮箱不匹配"); break;
            case ERRORCODES::PasswdUpFailed:    errStr = tr("密码更新失败，请重试"); break;
            default: errStr = tr("重置密码失败 (%1)").arg(rsp.error); break;
        }
        emit resetPasswordResult(false, errStr); return;
    }
    emit resetPasswordResult(true, tr("密码已重置，请登录"));
}
