#include "conuseritem.h"
#include "ui_conuseritem.h"

ConUserItem::ConUserItem(QWidget *parent)
    : ListItemBase(parent),
      ui(new Ui::ConUserItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CONTACT_USER_ITEM);
}

ConUserItem::~ConUserItem()
{
    delete ui;
}

QSize ConUserItem::sizeHint() const
{
    return QSize(250, 70);
}

void ConUserItem::setInfo(QString name, QString icon)
{
    name_ = name;
    icon_ = icon;
    ui->user_name_lb->setText(name_);
    // Set icon pixmap logic here
    QPixmap pixmap(icon_);
    ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ConUserItem::showRedPoint(bool show)
{
    ui->red_point->setVisible(show);
}
