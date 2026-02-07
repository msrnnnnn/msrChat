#include "sessionitemdelegate.h"
#include <QPainterPath>

SessionItemDelegate::SessionItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize SessionItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(250, 70); // 固定高度 70px (原需求 60px，适当增加以容纳内容)
}

void SessionItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;

    // 1. 绘制背景
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, QColor("#E5E6EB"));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(rect, QColor("#F5F6F7"));
    } else {
        painter->fillRect(rect, Qt::transparent);
    }

    // 获取数据
    QString name = index.data(Qt::DisplayRole).toString();
    QString msg = index.data(Qt::UserRole + 1).toString(); // 消息预览
    QPixmap avatar = index.data(Qt::DecorationRole).value<QPixmap>();
    QString time = index.data(Qt::UserRole + 2).toString(); // 时间戳

    // 2. 绘制头像 (40x40)
    QRect avatarRect(rect.left() + 10, rect.top() + 15, 40, 40);
    drawAvatar(painter, avatarRect, avatar);

    // 3. 绘制昵称
    painter->setPen(QColor("#333333"));
    painter->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    QRect nameRect(rect.left() + 60, rect.top() + 15, rect.width() - 110, 20);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, 
                      painter->fontMetrics().elidedText(name, Qt::ElideRight, nameRect.width()));

    // 4. 绘制时间
    painter->setPen(QColor("#999999"));
    painter->setFont(QFont("Microsoft YaHei", 9));
    QRect timeRect(rect.right() - 50, rect.top() + 15, 40, 20);
    painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, time);

    // 5. 绘制消息预览
    painter->setPen(QColor("#666666"));
    painter->setFont(QFont("Microsoft YaHei", 9));
    QRect msgRect(rect.left() + 60, rect.top() + 40, rect.width() - 80, 20);
    painter->drawText(msgRect, Qt::AlignLeft | Qt::AlignVCenter, 
                      painter->fontMetrics().elidedText(msg, Qt::ElideRight, msgRect.width()));

    painter->restore();
}

void SessionItemDelegate::drawAvatar(QPainter *painter, const QRect &rect, const QPixmap &avatar) const
{
    painter->save();
    QPainterPath path;
    path.addEllipse(rect);
    painter->setClipPath(path);

    if (avatar.isNull()) {
        // 默认头像
        painter->fillRect(rect, QColor("#CCCCCC"));
        painter->setPen(Qt::white);
        painter->setFont(QFont("Arial", 14, QFont::Bold));
        painter->drawText(rect, Qt::AlignCenter, "U");
    } else {
        painter->drawPixmap(rect, avatar);
    }
    painter->restore();
}
