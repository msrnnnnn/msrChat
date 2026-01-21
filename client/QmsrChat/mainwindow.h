#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "registerdialog.h"

/********************************************************************
* @file    mainwindow.h
* @brief   主窗口
*
* @author  马诗仁
* @date    2026/1/5
*
* @history
* 2026/1/5  马诗仁    修改内容
********************************************************************/

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void slotSwitchRegister();
    void slotSwitchLogin();

private:
    Ui::MainWindow *ui;

    LoginDialog *_login_dialog;
    RegisterDialog *_register_dialog;
};
#endif // MAINWINDOW_H
