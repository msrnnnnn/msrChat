/**
 * @file chatdialog.h
 * @brief 聊天对话框类
 * @details 仅负责承载输入框与 QML 视图，不处理聊天业务。
 */
#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include "ChatController.h"
#include "ChatListModel.h"
#include <QLineEdit>
#include <QQuickWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui
{
class ChatDialog;
}

class ChatDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

    ChatController *GetChatController() const { return _chat_controller; }

private:
    void SetupQmlView();

    Ui::ChatDialog *ui;
    QQuickWidget *_qml_widget;
    ChatListModel *_chat_model;
    ChatController *_chat_controller;
};

#endif
