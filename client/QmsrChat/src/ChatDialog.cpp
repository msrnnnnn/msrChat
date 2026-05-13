/**
 * @file ChatDialog.cpp
 * @brief 聊天对话框实现
 */
#include "ChatDialog.h"
#include "UserMgr.h"
#include "ui_chatdialog.h"
#include <QDebug>
#include <QQuickItem>

/**
 * @brief 构造函数
 * @details 初始化 ChatController、ChatListModel，并加载 QML 聊天界面
 */
ChatDialog::ChatDialog(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ChatDialog),
      _qml_widget(nullptr),
      _chat_model(new ChatListModel(this)),
      _chat_controller(new ChatController(this))
{
    ui->setupUi(this);

    _chat_controller->initialize();
    _chat_model->SetCurrentUid(UserMgr::Instance()->GetUid());
    _chat_controller->setChatModel(_chat_model);

    SetupQmlView();

    connect(
        ui->dest_uid_edit, &QLineEdit::editingFinished, this,
        [this]()
        {
            bool ok = false;
            const int uid = ui->dest_uid_edit->text().toInt(&ok);
            if (ok && uid > 0)
            {
                _chat_controller->setTargetUid(uid);
            }
        });
}

ChatDialog::~ChatDialog()
{
    delete ui;
}

/**
 * @brief 加载 QML 聊天视图
 * @details 将 ChatModel 和 ChatController 暴露给 QML，完成 UI 绑定
 */
void ChatDialog::SetupQmlView()
{
    _qml_widget = new QQuickWidget(this);
    _qml_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    _qml_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _qml_widget->setSource(QUrl(QStringLiteral("qrc:/ChatView.qml")));

    if (auto *root = _qml_widget->rootObject())
    {
        root->setProperty("chatModel", QVariant::fromValue(static_cast<QObject *>(_chat_model)));
        root->setProperty("chatController", QVariant::fromValue(static_cast<QObject *>(_chat_controller)));
    }
    else
    {
        qWarning() << "[ChatDialog] QML rootObject is null";
    }

    if (auto *layout = qobject_cast<QVBoxLayout *>(this->layout()))
    {
        layout->addWidget(_qml_widget, 1);
    }
}
