/**
 * @file ChatDialog.cpp
 * @brief 聊天对话框实现 - QML 混合架构版本
 */
#include "ChatDialog.h"
#include "Global.h"
#include "ui_chatdialog.h"
#include "UserMgr.h"
#include <QDateTime>
#include <QDebug>
#include <QQmlContext>
#include <QQmlEngine>
#include <QResizeEvent>

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ChatDialog),
      _qml_widget(nullptr),
      _chat_model(nullptr),
      _chat_controller(nullptr)
{
    ui->setupUi(this);

    DbThreadPool::Instance().Init();

    _chat_model = new ChatListModel(this);
    _chat_model->SetCurrentUid(UserMgr::Instance()->GetUid());

    _chat_controller = new ChatController(this);
    _chat_controller->initialize();

    SetupQmlView();

    connect(
        _chat_controller, &ChatController::sigMessageReceived, this, &ChatDialog::slotOnMessageReceived,
        Qt::QueuedConnection);
    connect(
        _chat_controller, &ChatController::sigMessageSent, this, &ChatDialog::slotOnMessageSent, Qt::QueuedConnection);
    connect(
        _chat_controller, &ChatController::sigMessageStatusChanged, this, &ChatDialog::slotOnMessageStatusChanged,
        Qt::QueuedConnection);
    connect(
        _chat_controller, &ChatController::sigHistoryLoaded, this, &ChatDialog::slotOnHistoryLoaded,
        Qt::QueuedConnection);
    connect(_chat_controller, &ChatController::sigError, this, &ChatDialog::slotOnError, Qt::QueuedConnection);
}

void ChatDialog::SetupQmlView()
{
    _qml_widget = new QQuickWidget(this);
    _qml_widget->setResizeMode(QQuickWidget::SizeRootObjectItem);
    _qml_widget->setSource(QUrl(QStringLiteral("qrc:/ChatView.qml")));

    QQmlContext *context = _qml_widget->rootContext();
    context->setContextProperty(QStringLiteral("chatModel"), _chat_model);
    context->setContextProperty(QStringLiteral("chatController"), _chat_controller);
    context->setContextProperty(QStringLiteral("chatDialog"), this);

    _qml_widget->setGeometry(ui->chat_list_view->geometry());
    _qml_widget->raise();
    _qml_widget->show();
}

ChatDialog::~ChatDialog()
{
    DbThreadPool::Instance().Shutdown();
    delete ui;
}

void ChatDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    if (_qml_widget && ui->chat_list_view)
    {
        _qml_widget->setGeometry(ui->chat_list_view->geometry());
    }
}

void ChatDialog::setTargetUid(int uid)
{
    if (_chat_controller)
    {
        _chat_controller->setTargetUid(uid);
    }
}

int ChatDialog::getTargetUid() const
{
    if (_chat_controller)
    {
        return _chat_controller->GetTargetUid();
    }
    return 0;
}

void ChatDialog::slotOnMessageReceived(const QVariantMap &msgData)
{
    bool isSelf = msgData.value("isSelf").toBool();
    AddMessageFromVariant(msgData, isSelf);
}

void ChatDialog::slotOnMessageSent(const QVariantMap &msgData)
{
    bool isSelf = true;
    AddMessageFromVariant(msgData, isSelf);
}

void ChatDialog::slotOnMessageStatusChanged(const QString &clientMsgId, int status)
{
    qDebug() << "[ChatDialog] Message status changed:" << clientMsgId << "status:" << status;
}

void ChatDialog::slotOnHistoryLoaded(const QVariantList &messages)
{
    qDebug() << "[ChatDialog] History loaded, count:" << messages.size();
}

void ChatDialog::slotOnError(const QString &error)
{
    qDebug() << "[ChatDialog] Error:" << error;
}

void ChatDialog::slotOnQmlSendMessage(const QString &content)
{
    if (_chat_controller)
    {
        _chat_controller->sendMessage(content);
    }
}

void ChatDialog::AddMessageFromVariant(const QVariantMap &msgData, bool isSelf)
{
    ChatMessage msg;
    msg.id = msgData.value("id").toLongLong();
    msg.from_uid = msgData.value("fromUid").toInt();
    msg.to_uid = msgData.value("toUid").toInt();
    msg.content = msgData.value("content").toString();
    msg.timestamp = msgData.value("timestamp").toLongLong();
    msg.status = msgData.value("status").toInt();

    _chat_model->AddMessage(msg);
}
