/**
 * @file chatdialog.h
 * @brief 聊天对话框类
 * @details 提供聊天界面，支持发送和接收消息。
 */
#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QHash>

namespace Ui {
class ChatDialog;
}

/**
 * @class ChatDialog
 * @brief 聊天对话框
 * @details 纯代码构建的聊天界面，支持消息收发。
 */
class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit ChatDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ChatDialog();

protected:
// 拦截事件的函数
bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /**
     * @brief 接收聊天消息槽
     * @param msg_id 消息 ID
     * @param data   消息数据
     */
    void slot_recv_chat_msg(quint16 msg_id, QByteArray data);

    /**
     * @brief 发送按钮点击槽
     */
    void slot_send_btn_clicked();

private:
    Ui::ChatDialog *ui;
    QHash<QString, QString> _pending_messages;
};

#endif // CHATDIALOG_H
