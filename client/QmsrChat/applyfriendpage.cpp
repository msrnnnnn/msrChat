#include "applyfriendpage.h"
#include "ui_applyfriendpage.h"
#include <QPainter>
#include <QStyleOption>

ApplyFriendPage::ApplyFriendPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ApplyFriendPage)
{
    ui->setupUi(this);

    // Test data
    // auto si = std::make_shared<SearchInfo>();
    // si->uid = 1001;
    // si->name = "TestUser";
    // si->desc = "Hello";
    // addNewApply(si);
}

ApplyFriendPage::~ApplyFriendPage()
{
    delete ui;
}

void ApplyFriendPage::addNewApply(std::shared_ptr<SearchInfo> si)
{
    if (applyItems_.contains(si->uid))
    {
        return;
    }

    auto *item = new ApplyFriendItem(ui->applyFriendList);
    item->setInfo(si, false);

    QListWidgetItem *list_item = new QListWidgetItem(ui->applyFriendList);
    list_item->setSizeHint(item->sizeHint());
    ui->applyFriendList->setItemWidget(list_item, item);

    applyItems_[si->uid] = item;

    connect(item, &ApplyFriendItem::sigAuthFriend, this, &ApplyFriendPage::slotAuthFriend);
}

void ApplyFriendPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ApplyFriendPage::slotAuthFriend(std::shared_ptr<SearchInfo> si)
{
    // Handle auth logic, maybe show a dialog or send request
    // For now just update UI
    if (applyItems_.contains(si->uid))
    {
        applyItems_[si->uid]->showAddBtn(false);
    }
}
