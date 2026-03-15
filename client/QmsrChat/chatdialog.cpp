/**
 * @file chatdialog.cpp
 * @brief 聊天对话框实现
 */
#include "chatdialog.h"
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
ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent)
{
    // 设置窗口标题
    setWindowTitle(tr("聊天窗口"));

    // 创建主布局
    QVBoxLayout *main_layout = new QVBoxLayout(this);

    // 顶部：聊天显示区域
    _chat_show = new QTextEdit(this);
    _chat_show->setReadOnly(true);
    main_layout->addWidget(_chat_show);

    // 中间：目标 UID 输入
    QHBoxLayout *dest_layout = new QHBoxLayout();
    QLabel *dest_label = new QLabel(tr("目标UID:"), this);
    _dest_uid_edit = new QLineEdit(this);
    dest_layout->addWidget(dest_label);
    dest_layout->addWidget(_dest_uid_edit);
    main_layout->addLayout(dest_layout);

    // 底部：消息输入和发送按钮
    QHBoxLayout *input_layout = new QHBoxLayout();
    _chat_edit = new QLineEdit(this);
    _send_btn = new QPushButton(tr("发送"), this);
    input_layout->addWidget(_chat_edit);
    input_layout->addWidget(_send_btn);
    main_layout->addLayout(input_layout);

    // 连接信号槽
    connect(
        TcpMgr::GetInstance(), static_cast<void (TcpMgr::*)(quint16, QByteArray)>(&TcpMgr::sig_msg_received), this,
        &ChatDialog::slot_recv_chat_msg);
    connect(_send_btn, &QPushButton::clicked, this, &ChatDialog::slot_send_btn_clicked);

    connect(
        TcpMgr::GetInstance(), &TcpMgr::sig_reconnected, this,
        [this]() { _chat_show->append(tr("[系统]: 网络已重连")); });
}

/**
 * @brief 析构函数
 */
ChatDialog::~ChatDialog()
{
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

        QString msg = tr("[用户 \"") + QString::number(from_uid) + tr("\"]: ") + content;
        _chat_show->append(msg);
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
            _chat_show->append(tr("[我]: ") + content);
        }
        else
        {
            if (message.isEmpty())
            {
                message = tr("发送失败");
            }
            _chat_show->append(tr("[系统]: ") + message);
        }
    }
}

/**
 * @brief 发送按钮点击处理
 */
void ChatDialog::slot_send_btn_clicked()
{
    QString content = _chat_edit->text();
    int to_uid = _dest_uid_edit->text().toInt();
    if (to_uid <= 0)
    {
        _chat_show->append(tr("[系统]: 目标UID无效"));
        return;
    }
    if (content.isEmpty())
    {
        return;
    }
    if (content.size() > 512)
    {
        _chat_show->append(tr("[系统]: 内容过长"));
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

    _chat_edit->clear();
}
