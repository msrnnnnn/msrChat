#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include "ProtocolStructs.h"
#include <QObject>
#include <QString>
#include <QVector>

class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggingIn READ isLoggingIn NOTIFY sigIsLoggingInChanged)
    Q_PROPERTY(bool chatLoginReady READ chatLoginReady NOTIFY sigChatLoginReadyChanged)

public:
    explicit AuthController(QObject *parent = nullptr);
    ~AuthController();

    Q_INVOKABLE QString validateUsername(const QString &username) const;
    Q_INVOKABLE QString validatePassword(const QString &password) const;
    Q_INVOKABLE QString validateEmail(const QString &email) const;
    Q_INVOKABLE QString validateConfirmPassword(const QString &password, const QString &confirm) const;
    Q_INVOKABLE QString validateVerifyCode(const QString &code) const;

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void sendRegisterVerifyCode(const QString &email);
    Q_INVOKABLE void registerUser(const QString &username, const QString &email,
                                   const QString &password, const QString &confirmPassword,
                                   const QString &verifyCode);
    Q_INVOKABLE void sendResetVerifyCode(const QString &email);
    Q_INVOKABLE void resetPassword(const QString &username, const QString &email,
                                    const QString &newPassword, const QString &verifyCode);

    bool isLoggingIn() const { return _is_logging_in; }
    bool chatLoginReady() const { return _chat_login_ready; }

    Q_INVOKABLE QVector<ChatTextMsgStruct> TakeBufferedMessages();

signals:
    void loginResult(bool success, const QString &message);
    void chatLoginSuccess();
    void tokenInvalid(const QString &message);
    void registerResult(bool success, const QString &message);
    void verifyCodeResult(bool success, const QString &message);
    void resetPasswordResult(bool success, const QString &message);
    void sigIsLoggingInChanged();
    void sigChatLoginReadyChanged();

private slots:
    void slotLoginRsp(const LoginRspStruct &rsp);
    void slotChatLoginRsp(const ChatLoginRspStruct &rsp);
    void slotRegisterRsp(const RegisterRspStruct &rsp);
    void slotVerifyCodeRsp(const VerifyCodeRspStruct &rsp);
    void slotResetPwdRsp(const ResetPwdRspStruct &rsp);

private:
    void ConnectTcpSignals();
    bool _is_logging_in = false;
    bool _chat_login_ready = false;
    int _uid = 0;
    QString _token;
    QVector<ChatTextMsgStruct> _buffered_messages;
    QMetaObject::Connection _msg_buffer_connection;
};

#endif // AUTHCONTROLLER_H
