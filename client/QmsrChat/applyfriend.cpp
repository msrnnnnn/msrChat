#include "applyfriend.h"
#include "ui_applyfriend.h"
#include <QDebug>
#include <QScrollBar>

ApplyFriend::ApplyFriend(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ApplyFriend),
      labelPoint_(0, 0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);

    ui->lb_ed->show();

    connect(ui->apply_cancel, &QPushButton::clicked, this, &ApplyFriend::slotApplyCancel);
    connect(ui->apply_sure, &QPushButton::clicked, this, &ApplyFriend::slotApplySure);
    connect(ui->more_lb, &ClickedLabel::clicked, this, &ApplyFriend::showMoreLabel);
    connect(ui->lb_ed, &QLineEdit::returnPressed, this, &ApplyFriend::slotLabelEnter);
    connect(ui->lb_ed, &QLineEdit::textChanged, this, &ApplyFriend::slotLabelTextChange);
    connect(ui->lb_ed, &QLineEdit::editingFinished, this, &ApplyFriend::slotLabelEditFinished);

    initTipLbs();
}

ApplyFriend::~ApplyFriend()
{
    delete ui;
}

void ApplyFriend::initTipLbs()
{
    tipData_ = {QStringLiteral("同学"), QStringLiteral("家人"),   QStringLiteral("菜鸟教程"),
                QStringLiteral("C++"),  QStringLiteral("Python"), QStringLiteral("Java")};
    tipCurPoint_ = QPoint(0, 0);

    for (const auto &tip : tipData_)
    {
        auto lb = new ClickedLabel(ui->input_tip_wid);
        lb->setText(tip);
        lb->adjustSize();
        lb->SetState(
            QStringLiteral("normal"), QStringLiteral("hover"), QStringLiteral("press"), QStringLiteral("selected"),
            QStringLiteral("selected_hover"), QStringLiteral("selected_press"));
        connect(lb, &ClickedLabel::clicked, this, &ApplyFriend::slotChangeFriendLabelByTip);

        QPoint next_point;
        addTipLbs(lb, tipCurPoint_, next_point, lb->width(), lb->height());
        tipCurPoint_ = next_point;

        addLabels_[tip] = lb;
        addLabelKeys_.push_back(tip);
    }
}

void ApplyFriend::addTipLbs(ClickedLabel *lb, QPoint cur_point, QPoint &next_point, int text_width, int text_height)
{
    lb->move(cur_point);
    lb->show();

    int x = cur_point.x() + text_width + 10;
    int y = cur_point.y();

    if (x + text_width > ui->input_tip_wid->width())
    {
        x = 0;
        y += text_height + 10;
    }

    next_point = QPoint(x, y);
}

bool ApplyFriend::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->lb_ed && event->type() == QEvent::KeyPress)
    {
        // Handle keys if needed
    }
    return QDialog::eventFilter(obj, event);
}

void ApplyFriend::setSearchInfo(std::shared_ptr<SearchInfo> si)
{
    si_ = si;
    if (si_)
    {
        ui->lb_title->setText(QString("Apply to add %1").arg(si_->name));
    }
}

void ApplyFriend::resetLabels()
{
    for (auto lb : friendLabels_)
    {
        lb->deleteLater();
    }
    friendLabels_.clear();
    friendLabelKeys_.clear();
    labelPoint_ = QPoint(0, 0);
}

void ApplyFriend::addLabel(QString name)
{
    if (friendLabels_.contains(name))
        return;
    if (friendLabelKeys_.size() >= 10)
        return;

    auto lb = new FriendLabel(ui->gridWidget);
    lb->setText(name);
    lb->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 2px;");
    lb->adjustSize();

    int width = lb->width();
    int height = lb->height();

    if (labelPoint_.x() + width > ui->gridWidget->width())
    {
        labelPoint_.setX(0);
        labelPoint_.setY(labelPoint_.y() + height + 10);
    }

    lb->move(labelPoint_);
    lb->show();

    labelPoint_.setX(labelPoint_.x() + width + 10);

    friendLabels_[name] = lb;
    friendLabelKeys_.push_back(name);

    connect(lb, &ClickedOnceLabel::clicked, this, &ApplyFriend::slotRemoveFriendLabel);
}

void ApplyFriend::showMoreLabel()
{
    // Logic to show more labels
}

void ApplyFriend::slotLabelEnter()
{
    QString text = ui->lb_ed->text();
    if (text.isEmpty())
        return;
    addLabel(text);
    ui->lb_ed->clear();
}

void ApplyFriend::slotRemoveFriendLabel(QString name)
{
    if (friendLabels_.contains(name))
    {
        auto lb = friendLabels_[name];
        lb->deleteLater();
        friendLabels_.remove(name);

        // Remove from keys
        auto it = std::find(friendLabelKeys_.begin(), friendLabelKeys_.end(), name);
        if (it != friendLabelKeys_.end())
        {
            friendLabelKeys_.erase(it);
        }

        if (addLabels_.contains(name))
        {
            // addLabels_[name]->SetState(ClickLbState::Normal);
        }
    }
}

void ApplyFriend::slotChangeFriendLabelByTip(QString text, ClickLbState state)
{
    if (state == ClickLbState::Selected)
    {
        addLabel(text);
    }
    else
    {
        slotRemoveFriendLabel(text);
    }
}

void ApplyFriend::slotLabelTextChange(const QString &text)
{
    //
}

void ApplyFriend::slotLabelEditFinished()
{
    slotLabelEnter();
}

void ApplyFriend::slotAddFirendLabelByClickTip(QString text)
{
    addLabel(text);
}

void ApplyFriend::slotApplyCancel()
{
    this->reject();
}

void ApplyFriend::slotApplySure()
{
    // Logic to send apply friend request
    this->accept();
}
