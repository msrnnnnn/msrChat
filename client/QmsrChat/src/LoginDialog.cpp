/**
 * @file LoginDialog.cpp
 * @brief 登录对话框实现
 */
#include "LoginDialog.h"
#include "DPIHelper.h"
#include "Global.h"
#include "TcpMgr.h"
#include "ui_logindialog.h"
#include "UserMgr.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

static constexpr int DEV_BTN_WIDTH = 80;
static constexpr int DEV_BTN_HEIGHT = 30;
static constexpr int DEV_BTN_MARGIN = 10;

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
    repolish(ui->error_label);

    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::on_login_Button_clicked);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    connect(TcpMgr::Instance(), &TcpMgr::sig_login_rsp, this, &LoginDialog::slot_login_rsp);

    connect(TcpMgr::Instance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    connect(TcpMgr::Instance(), &TcpMgr::sig_chat_login_rsp, this, &LoginDialog::slot_chat_login_rsp);

    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    ui->password_Edit->setEchoMode(QLineEdit::Password);
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->pass_visible->setText(tr("显示"));
    connect(
        ui->pass_visible, &ClickedLabel::clicked, this,
        [this]()
        {
            auto state = ui->pass_visible->GetCurState();
            if (state == ClickLbState::Normal)
            {
                ui->password_Edit->setEchoMode(QLineEdit::Password);
                ui->pass_visible->setText(tr("显示"));
            }
            else
            {
                ui->password_Edit->setEchoMode(QLineEdit::Normal);
                ui->pass_visible->setText(tr("隐藏"));
            }
        });

    QPushButton *devBtn = new QPushButton(tr("开发模式"), this);

    QSize scaledBtnSize = DPI.scaledSize(DEV_BTN_WIDTH, DEV_BTN_HEIGHT);
    devBtn->setMinimumSize(scaledBtnSize);
    devBtn->setMaximumSize(scaledBtnSize);

    DPI.applySizePolicy(devBtn, false, false);

    int dev_x = std::max(DPI.scaled(DEV_BTN_MARGIN), width() - scaledBtnSize.width() - DPI.scaled(DEV_BTN_MARGIN));
    int dev_y = DPI.scaled(340);
    devBtn->setGeometry(dev_x, dev_y, scaledBtnSize.width(), scaledBtnSize.height());

    devBtn->setStyleSheet(
        "QPushButton { background-color: #FF9800; color: white; border: none; padding: 5px; }"
        "QPushButton:hover { background-color: #F57C00; }");
    devBtn->show();

    connect(
        devBtn, &QPushButton::clicked, this,
        [this]()
        {
            qDebug() << "Dev Mode activated - TCP already connected at startup";

            int devUid = 1001;
            QString devToken = "dev_token";
            UserMgr::Instance()->SetUid(devUid);
            UserMgr::Instance()->SetToken(devToken);
            _uid = devUid;
            _token = devToken;
            _chat_login_ready = false;

            showTip(tr("开发模式：正在登录聊天服务..."), true);

            ChatLoginReqStruct req;
            req.uid = _uid;
            req.token = _token;
            TcpMgr::Instance()->slot_send_chat_login_req(req);
        });
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
    auto user = ui->user_Edit->text();
    if (user.isEmpty())
    {
        qDebug() << "User empty ";
        return false;
    }
    return true;
}

bool LoginDialog::checkPwdValid()
{
    auto pwd = ui->password_Edit->text();
    if (pwd.length() < 6 || pwd.length() > 15)
    {
        qDebug() << "Pass length invalid";
        return false;
    }
    return true;
}

/**
 * @brief 登录按钮点击处理
 */
void LoginDialog::on_login_Button_clicked()
{
    if (checkUserValid() == false)
    {
        showTip(tr("用户名不能为空"), false);
        return;
    }
    if (checkPwdValid() == false)
    {
        showTip(tr("密码长度应为6~15"), false);
        return;
    }
    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();
    LoginReqStruct req;
    req.user = user;
    req.passwd = hashPassword(pwd);
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
    if (isCorrect)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "error");
    }
    ui->error_label->setText(str);
    repolish(ui->error_label);
}
