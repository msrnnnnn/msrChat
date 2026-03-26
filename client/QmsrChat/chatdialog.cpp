/**
 * @file chatdialog.cpp
 * @brief 聊天对话框实现
 */
#include "chatdialog.h"
#include "ui_chatdialog.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

/**
 * @brief 构造函数
 * @param parent 父窗口
 */
ChatDialog::ChatDialog(QWidget *parent) : QDialog(parent), ui(new Ui::ChatDialog)
{
    ui->setupUi(this);
    // 把按钮的父对象改成输入框
    ui->pushButton->setParent(ui->chat_edit);
    // 使按钮显示在最上层
    ui->pushButton->raise();
    ui->chat_edit->installEventFilter(this);
           // 连接信号槽
    connect(TcpMgr::GetInstance(), static_cast<void (TcpMgr::*)(quint16, QByteArray)>(&TcpMgr::sig_msg_received), this,
            &ChatDialog::slot_recv_chat_msg);
    connect(ui->pushButton, &QPushButton::clicked, this, &ChatDialog::slot_send_btn_clicked);

    connect(TcpMgr::GetInstance(), &TcpMgr::sig_reconnected, this,
            [this]() { ui->chat_show->append(tr("[系统]: 网络已重连")); });
}

/**
 * @brief 析构函数
 */
ChatDialog::~ChatDialog()
{
    delete ui;
}

/**
 * @brief 接收聊天消息并更新显示
 * @param msg_id 消息类型
 * @param data 消息体数据
 */
void ChatDialog::slot_recv_chat_msg(quint16 msg_id, QByteArray data)
{
    if (msg_id == static_cast<quint16>(RequestType::MSG_CHAT_TEXT))
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
        {
            qDebug() << "Failed to parse chat message JSON";
            return;
        }

        QJsonObject obj = doc.object();
        int from_uid = obj["from_uid"].toInt();
        QString content = obj["content"].toString();

        QString msg = QString("<div style='text-align: left; color: #333333;'>[用户 %1]: %2</div>").arg(from_uid).arg(content);
        ui->chat_show->append(msg);
        return;
    }
    if (msg_id == static_cast<quint16>(RequestType::MSG_CHAT_ACK))
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
        {
            qDebug() << "Failed to parse ack JSON";
            return;
        }
        QJsonObject obj = doc.object();
        int error = obj.value("error").toInt(1);
        QString message = obj.value("message").toString();
        QString client_msg_id = obj.value("client_msg_id").toString();
        if (client_msg_id.isEmpty())
        {
            return;
        }
        QString content = _pending_messages.take(client_msg_id);
        if (content.isEmpty())
        {
            return;
        }
        if (error == 0)
        {
            ui->chat_show->append(QString("<div style='text-align: right; color: #07C160;'>%1 :[我]</div>").arg(content));
        }
        else
        {
            if (message.isEmpty())
            {
                message = tr("发送失败");
            }
            ui->chat_show->append(tr("[系统]: ") + message);
        }
    }
}

/**
 * @brief 发送按钮点击处理
 */
void ChatDialog::slot_send_btn_clicked()
{
    QString content = ui->chat_edit->toPlainText();
    int to_uid = ui->dest_uid_edit->text().toInt();
    if (to_uid <= 0)
    {
        ui->chat_show->append(tr("[系统]: 目标UID无效"));
        return;
    }
    if (content.isEmpty())
    {
        return;
    }
    if (content.size() > 512)
    {
        ui->chat_show->append(tr("[系统]: 内容过长"));
        return;
    }

    int from_uid = UserMgr::GetInstance()->GetUid();
    QString client_msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject obj;
    obj["from_uid"] = from_uid;
    obj["to_uid"] = to_uid;
    obj["content"] = content;
    obj["client_msg_id"] = client_msg_id;

    QJsonDocument doc(obj);
    QByteArray json_data = doc.toJson(QJsonDocument::Compact);

    _pending_messages.insert(client_msg_id, content);
    TcpMgr::GetInstance()->slot_send_data(RequestType::MSG_CHAT_TEXT, QString(json_data));

    ui->chat_edit->clear();
}


bool ChatDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 如果是输入框的大小发生了改变
    if (watched == ui->chat_edit && event->type() == QEvent::Resize) {
        // 动态计算按钮的位置：输入框宽度 - 按钮宽度 - 10像素右边距
        int x = ui->chat_edit->width() - ui->pushButton->width() - 10;
        int y = ui->chat_edit->height() - ui->pushButton->height() - 10;
        ui->pushButton->move(x, y);
    }
    return QDialog::eventFilter(watched, event);
}
