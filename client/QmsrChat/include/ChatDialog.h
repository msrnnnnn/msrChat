/**
 * @file chatdialog.h
 * @brief 聊天对话框类
 * @details 使用 QQuickWidget 嵌入 QML 聊天视图，保留原有 QWidget 窗口框架。
 *         通过 ChatController 控制器桥接 QML 与 C++ 业务逻辑。
 */
#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include "ChatController.h"
#include "ChatListModel.h"
#include "ProtocolStructs.h"
#include <QDialog>
#include <QQuickWidget>
#include <QResizeEvent>
#include <QShowEvent>

namespace Ui
{
class ChatDialog;
}

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

    Q_INVOKABLE void setTargetUid(int uid);
    Q_INVOKABLE int getTargetUid() const;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void slotOnMessageReceived(const QVariantMap &msgData);
    void slotOnMessageSent(const QVariantMap &msgData);
    void slotOnMessageStatusChanged(const QString &clientMsgId, int status);
    void slotOnHistoryLoaded(const QVariantList &messages);
    void slotOnError(const QString &error);
    void slotOnQmlSendMessage(const QString &content);

private:
    void SetupQmlView();
    void AddMessageFromVariant(const QVariantMap &msgData, bool isSelf);

    Ui::ChatDialog *ui;
    QQuickWidget *_qml_widget;
    ChatListModel *_chat_model;
    ChatController *_chat_controller;
    QVariantList _pending_messages;
};

#endif
