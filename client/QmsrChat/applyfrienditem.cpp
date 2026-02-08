#include "applyfrienditem.h"
#include "ui_applyfrienditem.h"

ApplyFriendItem::ApplyFriendItem(QWidget *parent)
    : ListItemBase(parent),
      ui(new Ui::ApplyFriendItem),
      added_(false)
{
    ui->setupUi(this);
    SetItemType(ListItemType::APPLY_FRIEND_ITEM);
}

ApplyFriendItem::~ApplyFriendItem()
{
    delete ui;
}

void ApplyFriendItem::setInfo(std::shared_ptr<SearchInfo> si, bool added)
{
    si_ = si;
    added_ = added;

    if (si_)
    {
        ui->userName->setText(si_->name);
        ui->userMsg->setText(si_->desc); // Assuming desc is message
        // Set icon
    }

    if (added_)
    {
        ui->addBtn->hide();
        ui->addedLb->show();
    }
    else
    {
        ui->addBtn->show();
        ui->addedLb->hide();
    }
}

void ApplyFriendItem::showAddBtn(bool show)
{
    if (show)
    {
        ui->addBtn->show();
        ui->addedLb->hide();
    }
    else
    {
        ui->addBtn->hide();
        ui->addedLb->show();
    }
}

QSize ApplyFriendItem::sizeHint() const
{
    return QSize(250, 80);
}

int ApplyFriendItem::getUid()
{
    if (si_)
        return si_->uid;
    return 0;
}

void ApplyFriendItem::on_addBtn_clicked()
{
    emit sigAuthFriend(si_);
}
