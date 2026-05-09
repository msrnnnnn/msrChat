/**
 * @file LoginDialog.cpp
 * @brief 登录对话框实现
 */
#include "LoginDialog.h"
#include "AuthUiHelpers.h"
#include "DPIHelper.h"
#include "TcpMgr.h"
#include "Utils.h"
#include "ui_logindialog.h"
#include "UserMgr.h"
#include <QDebug>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    ui->error_label->setProperty("state", "normal");
    Utils::repolish(ui->error_label);

    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::on_login_Button_clicked);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    connect(TcpMgr::Instance(), &TcpMgr::sig_login_rsp, this, &LoginDialog::slot_login_rsp);

    connect(TcpMgr::Instance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    connect(TcpMgr::Instance(), &TcpMgr::sig_chat_login_rsp, this, &LoginDialog::slot_chat_login_rsp);

    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    AuthUiHelpers::BindPasswordToggle(ui->pass_visible, ui->password_Edit);
}

/**
 * @brief 析构函数
 */
LoginDialog::~LoginDialog()
{
    delete ui;
}

bool LoginDialog::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);

#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_NCHITTEST)
    {
        *result = HTCLIENT;
        return true;
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return QDialog::nativeEvent(eventType, message, result);
}

bool LoginDialog::checkUserValid()
{
    return AuthUiHelpers::ValidateUsername(ui->user_Edit->text()).isEmpty();
}

bool LoginDialog::checkPwdValid()
{
    return AuthUiHelpers::ValidatePassword(ui->password_Edit->text()).isEmpty();
}

/**
 * @brief 登录按钮点击处理
 */
void LoginDialog::on_login_Button_clicked()
{
    if (checkUserValid() == false)
    {
        showTip(AuthUiHelpers::ValidateUsername(ui->user_Edit->text()), false);
        return;
    }
    if (checkPwdValid() == false)
    {
        showTip(AuthUiHelpers::ValidatePassword(ui->password_Edit->text()), false);
        return;
    }
    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();
    LoginReqStruct req;
    req.user = user;
    req.passwd = Utils::hashPassword(pwd);
    TcpMgr::Instance()->slot_send_login_req(req);
}

/**
 * @brief TCP 认证回包处理
 * @param req_type 请求类型
 * @param data 响应数据
 */
void LoginDialog::slot_login_rsp(const LoginRspStruct &rsp)
{
    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
        QString errStr = tr("登录失败");
        switch (static_cast<ERRORCODES>(rsp.error))
        {
            case ERRORCODES::PasswdErr:
                errStr = tr("密码错误");
                break;
            case ERRORCODES::UserNotExist:
                errStr = tr("用户不存在");
                break;
            default:
                errStr = tr("登录失败，错误码: ") + QString::number(rsp.error);
                break;
        }
        showTip(errStr, false);
        return;
    }

    _uid = rsp.uid;
    _token = rsp.token;
    UserMgr::Instance()->SetUid(_uid);
    UserMgr::Instance()->SetToken(_token);

    showTip(tr("登录成功，正在连接聊天服务..."), true);

    ChatLoginReqStruct req;
    req.uid = _uid;
    req.token = _token;
    TcpMgr::Instance()->slot_send_chat_login_req(req);
}

/**
 * @brief 忘记密码点击处理
 */
void LoginDialog::slot_forget_pwd()
{
    emit switchReset();
}

/**
 * @brief TCP 连接完成回调
 * @param bsuccess 是否连接成功
 * @note 此函数主要用于断线重连场景。正常登录流程中，TCP 连接在启动时已建立，
 *       MSG_CHAT_LOGIN 在 ID_LOGIN_USER 成功后直接发送，无需等待此回调。
 */
void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if (bsuccess)
    {
        qDebug() << "TCP connection established (reconnection or startup)";
        if (_uid > 0 && !_token.isEmpty() && !_chat_login_ready)
        {
            showTip(tr("重连成功，正在恢复聊天会话..."), true);
            ChatLoginReqStruct req;
            req.uid = _uid;
            req.token = _token;
            TcpMgr::Instance()->slot_send_chat_login_req(req);
        }
        return;
    }

    showTip(tr("聊天服务连接失败，请检查网络"), false);
}

/**
 * @brief 聊天登录回包处理
 * @param rsp 强类型聊天登录响应结构体
 */
void LoginDialog::slot_chat_login_rsp(const ChatLoginRspStruct &rsp)
{
    qDebug() << "LoginDialog received chat login rsp, error:" << rsp.error << "message:" << rsp.message;

    if (rsp.error != 0)
    {
        QString message = rsp.message.isEmpty() ? tr("聊天登录失败") : rsp.message;
        qWarning() << "Chat login failed:" << message;
        showTip(message, false);
        return;
    }

    if (!_chat_login_ready)
    {
        _chat_login_ready = true;
        showTip(tr("聊天登录成功，正在进入聊天界面..."), true);
        emit sig_login_success();
    }
}

/**
 * @brief 显示提示信息
 * @param str 提示文本
 * @param isCorrect 是否为成功提示
 */
void LoginDialog::showTip(QString str, bool isCorrect)
{
    AuthUiHelpers::ShowTip(ui->error_label, str, isCorrect);
}
