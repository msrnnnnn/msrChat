#include "contactuserlist.h"
#include <QDebug>
#include <QEvent>
#include <QScrollBar>

ContactUserList::ContactUserList(QWidget *parent)
    : QListWidget(parent),
      addFriendItem_(nullptr),
      groupItem_(nullptr)
{
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->viewport()->installEventFilter(this);

    connect(this, &QListWidget::itemClicked, this, &ContactUserList::slotItemClicked);

    addContactUserList();
}

void ContactUserList::showRedPoint(bool bshow)
{
    addFriendItem_->showRedPoint(bshow);
}

bool ContactUserList::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this->viewport())
    {
        if (event->type() == QEvent::Enter)
        {
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        else if (event->type() == QEvent::Leave)
        {
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

    if (watched == this->verticalScrollBar())
    {
        if (event->type() == QEvent::Enter)
        {
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        else if (event->type() == QEvent::Leave)
        {
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

    return QListWidget::eventFilter(watched, event);
}

void ContactUserList::addContactUserList()
{
    // Add "New Friends" item
    auto *item = new QListWidgetItem(this);
    addFriendItem_ = new ConUserItem(this);
    addFriendItem_->setInfo("New Friends", ":/res/add_friend_normal.png");
    item->setSizeHint(addFriendItem_->sizeHint());
    this->setItemWidget(item, addFriendItem_);

    // Add "Group" item
    groupItem_ = new QListWidgetItem(this);
    auto *groupItemWid = new ConUserItem(this);
    groupItemWid->setInfo("Groups", ":/res/filedir.png");
    groupItem_->setSizeHint(groupItemWid->sizeHint());
    this->setItemWidget(groupItem_, groupItemWid);

    emit sigLoadingContactUser();
}

void ContactUserList::slotItemClicked(QListWidgetItem *item)
{
    if (item == this->item(0))
    { // New Friends
        addFriendItem_->showRedPoint(false);
        emit sigSwitchApplyFriendPage();
    }
    else
    {
        emit sigSwitchFriendInfoPage();
    }
}
