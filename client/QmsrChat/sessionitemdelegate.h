#ifndef SESSIONITEMDELEGATE_H
#define SESSIONITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>

class SessionItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SessionItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    void drawAvatar(QPainter *painter, const QRect &rect, const QPixmap &avatar) const;
};

#endif // SESSIONITEMDELEGATE_H
