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
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_msg_received, this, &ChatDialog::slot_recv_chat_msg);
    connect(_send_btn, &QPushButton::clicked, this, &ChatDialog::slot_send_btn_clicked);

    // ========== 自动发送 1005 绑定包 ==========
    // 在对话框显示时，自动向服务器绑定 UID
    int uid = UserMgr::GetInstance()->GetUid();
    QString token = UserMgr::GetInstance()->GetToken();
    if (uid > 0) {
        QJsonObject bindObj;
        bindObj["uid"] = uid;
        bindObj["token"] = token;
        QJsonDocument bindDoc(bindObj);
        QString bindString = bindDoc.toJson(QJsonDocument::Compact);
        TcpMgr::GetInstance()->slot_send_data(1005, bindString);
        qDebug() << "ChatDialog: Auto-sent 1005 binding packet for UID:" << uid;
        _chat_show->append(tr("[系统]: 已连接聊天服务"));
    } else {
        qDebug() << "ChatDialog: Warning - UID is invalid, cannot send binding packet";
        _chat_show->append(tr("[系统]: 警告 - 用户ID无效"));
    }

    // 连接重连信号 - 断线重连后重新发送登录包
    connect(
        TcpMgr::GetInstance().get(), &TcpMgr::sig_reconnected, this,
        [this]()
        {
            qDebug() << "Detected reconnection, resending login packet (1005)";

            // 获取当前用户信息
            int uid = UserMgr::GetInstance()->GetUid();
            QString token = UserMgr::GetInstance()->GetToken();

            if (uid <= 0)
            {
                qDebug() << "Warning: UID is invalid, cannot resend login packet";
                return;
            }

            // 构建登录包 JSON
            QJsonObject login_obj;
            login_obj["uid"] = uid;
            login_obj["token"] = token;

            QJsonDocument doc(login_obj);
            QByteArray json_data = doc.toJson(QJsonDocument::Compact);

            // 发送 1005 登录包
            TcpMgr::GetInstance()->slot_send_data(static_cast<RequestType>(1005), QString(json_data));

            qDebug() << "Login packet (1005) resent for UID:" << uid;

            // 在聊天窗口显示重连提示
            _chat_show->append(tr("[系统]: 网络已重连，已重新登录"));
        });
}

ChatDialog::~ChatDialog()
{
}

void ChatDialog::slot_recv_chat_msg(quint16 msg_id, QByteArray data)
{
    if (msg_id == 1006)
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
    }
}

void ChatDialog::slot_send_btn_clicked()
{
    QString content = _chat_edit->text();
    if (content.isEmpty())
    {
        return;
    }

    int to_uid = _dest_uid_edit->text().toInt();
    int from_uid = UserMgr::GetInstance()->GetUid();

    QJsonObject obj;
    obj["from_uid"] = from_uid;
    obj["to_uid"] = to_uid;
    obj["content"] = content;

    QJsonDocument doc(obj);
    QByteArray json_data = doc.toJson(QJsonDocument::Compact);

    TcpMgr::GetInstance()->slot_send_data(1006, QString(json_data));

    QString display_msg = tr("[我]: ") + content;
    _chat_show->append(display_msg);

    _chat_edit->clear();
}
