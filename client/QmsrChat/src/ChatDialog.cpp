/**
 * @file ChatDialog.cpp
 * @brief 聊天对话框实现 - QML 混合架构版本
 */
#include "ChatDialog.h"
#include "Global.h"
#include "UserMgr.h"
#include "ui_chatdialog.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QQmlEngine>
#include <QQuickItem>
#include <QShowEvent>
#include <QVariantMap>

ChatDialog::ChatDialog(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ChatDialog),
      _qml_widget(nullptr),
      _chat_model(nullptr),
      _chat_controller(nullptr)
{
    ui->setupUi(this);

    QString app_path = QCoreApplication::applicationDirPath();
    QString db_path = QDir::toNativeSeparators(app_path + QDir::separator() + "chat_messages.db");
    DbThreadPool::Instance().Init(db_path);

    _chat_model = new ChatListModel(this);
    _chat_model->SetCurrentUid(UserMgr::Instance()->GetUid());

    _chat_controller = new ChatController(this);
    _chat_controller->initialize();
    // 开发模式默认目标为 1，普通模式默认目标为 1001
    int current_uid = UserMgr::Instance()->GetUid();
    int default_target = (current_uid == 1001) ? 1 : 1001;
    _chat_controller->setTargetUid(default_target);

    SetupQmlView();

    // 连接目标UID输入框到ChatController
    connect(ui->dest_uid_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        int uid = ui->dest_uid_edit->text().toInt(&ok);
        if (ok && uid > 0) {
            _chat_controller->setTargetUid(uid);
        }
    });

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
    _qml_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    _qml_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _qml_widget->setMinimumSize(200, 200);

    _qml_widget->setSource(QUrl(QStringLiteral("qrc:/ChatView.qml")));

    auto status = _qml_widget->status();
    if (status == QQuickWidget::Error)
        qWarning() << "[ChatDialog] QML load errors:" << _qml_widget->errors();
    else if (status == QQuickWidget::Null)
        qWarning() << "[ChatDialog] QML source not set or null";

    if (auto *root = _qml_widget->rootObject())
    {
        root->setProperty("chatModel", QVariant::fromValue(static_cast<QObject *>(_chat_model)));
        root->setProperty("chatController", QVariant::fromValue(static_cast<QObject *>(_chat_controller)));
        root->setProperty("chatDialog", QVariant::fromValue(static_cast<QObject *>(this)));
    }
    else
    {
        qWarning() << "[ChatDialog] QML rootObject is null";
    }

    // 彻底重建布局：保存 UID 行控件后删除旧布局，创建新布局避免布局损坏
    QWidget *uidLabel = ui->label;
    QWidget *uidEdit = ui->dest_uid_edit;
    QWidget *oldList = ui->chat_list_view;
    QWidget *oldEdit = ui->chat_edit;
    QWidget *oldBtn = ui->pushButton;

    delete this->layout();

    QVBoxLayout *newLayout = new QVBoxLayout(this);
    newLayout->setContentsMargins(11, 11, 11, 11);
    newLayout->setSpacing(0);

    QHBoxLayout *uidRow = new QHBoxLayout();
    uidRow->setContentsMargins(0, 0, 0, 8);
    uidRow->addWidget(uidLabel);
    uidRow->addWidget(uidEdit);
    newLayout->addLayout(uidRow);

    newLayout->addWidget(_qml_widget, 1);

    oldList->hide();
    oldEdit->hide();
    oldBtn->hide();

    qDebug() << "[ChatDialog] Layout rebuilt, QML widget size:" << _qml_widget->size();
}

ChatDialog::~ChatDialog()
{
    DbThreadPool::Instance().Shutdown();
    delete ui;
}

void ChatDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (_qml_widget)
    {
        // 在首次显示时打印实际尺寸和状态，便于排查渲染问题
        qDebug() << "[ChatDialog] showEvent — ChatDialog size:" << size()
                 << "QML widget size:" << _qml_widget->size()
                 << "QML status:" << _qml_widget->status()
                 << "rootObject:" << (_qml_widget->rootObject() != nullptr);
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
    if (_chat_model)
    {
        _chat_model->UpdateMessageStatus(qHash(clientMsgId), status);
    }
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
