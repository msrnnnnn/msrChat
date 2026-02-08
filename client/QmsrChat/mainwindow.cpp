/**
 * @file    mainwindow.cpp
 * @brief   主窗口实现
 * @author  msr
 */

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "tcpmgr.h"
#include <QIcon>

/**
 * @brief 构造函数
 * @details 初始化窗口、子对话框及信号连接。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. 主窗口基础设置
    this->setWindowIcon(QIcon(":/image/mainwidow.ico"));
    setFixedSize(300, 450);

    // 隐藏 MainWindow 默认的中心控件（避免显示空白）
    if (this->centralWidget())
    {
        this->centralWidget()->hide();
    }

    // 2. 初始化登录对话框
    _login_dialog = new LoginDialog(this); // 设父对象，依附主窗口
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _login_dialog->setFixedSize(this->size());
    _login_dialog->move(0, 0);
    _login_dialog->show();

    // 3. 初始化注册对话框
    _register_dialog = new RegisterDialog(this);
    _register_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _register_dialog->setFixedSize(this->size());
    _register_dialog->move(0, 0);
    _register_dialog->hide();

    _reset_dialog = nullptr;
    _chat_dialog = nullptr;

    // 4. 绑定切换信号槽
    connect(_login_dialog, &LoginDialog::switchRegister, this, &MainWindow::slotSwitchRegister);
    connect(_register_dialog, &RegisterDialog::switchLogin, this, &MainWindow::slotSwitchLogin);
    // 连接登录界面忘记密码信号
    connect(_login_dialog, &LoginDialog::switchReset, this, &MainWindow::slotSwitchReset);

    // 连接创建聊天界面信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_swich_chatdlg, this, &MainWindow::slotSwitchChat);

    // 测试跳转
    emit TcpMgr::GetInstance() -> sig_swich_chatdlg();
}

/**
 * @brief 析构函数
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief 切换到注册界面
 */
void MainWindow::slotSwitchRegister()
{
    _login_dialog->hide();
    _register_dialog->show();
}

/**
 * @brief 切换回登录界面
 */
void MainWindow::slotSwitchLogin()
{
    _register_dialog->hide();
    _login_dialog->show();
}

void MainWindow::slotSwitchReset()
{
    // 创建 ResetDialog (如果尚未创建)
    if (!_reset_dialog)
    {
        _reset_dialog = new ResetDialog(this);
        _reset_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
        _reset_dialog->setFixedSize(this->size());
        _reset_dialog->move(0, 0);

        // 注册返回登录信号和槽函数
        connect(_reset_dialog, &ResetDialog::switchLogin, this, &MainWindow::slotSwitchLogin2);
    }

    _reset_dialog->show();
    _login_dialog->hide();
    _register_dialog->hide();
}

/**
 * @brief 从重置密码界面切换回登录界面
 */
void MainWindow::slotSwitchLogin2()
{
    _reset_dialog->hide();
    _login_dialog->show();
}

/**
 * @brief 切换到聊天界面
 */
void MainWindow::slotSwitchChat()
{
    _chat_dialog = new ChatDialog();
    _chat_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_chat_dialog);
    _chat_dialog->show();
    _login_dialog->hide();
    this->setMinimumSize(QSize(1050, 900));
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}
