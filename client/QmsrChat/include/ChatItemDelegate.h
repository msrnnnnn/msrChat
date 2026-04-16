#ifndef CHATITEMDELEGATE_H
#define CHATITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QRect>
#include <QSize>
#include <QMap>

class ChatItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ChatItemDelegate(QObject* parent = nullptr);
    ~ChatItemDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void SetViewportWidth(int width);

signals:

public slots:

private:
    QSize CalculateBubbleSize(const QString& content, bool isSelf, int viewportWidth) const;
    void DrawBubble(QPainter* painter, const QRect& bubbleRect, bool isSelf) const;
    void DrawText(QPainter* painter, const QRect& textRect, const QString& text, bool isSelf) const;
    void DrawAvatar(QPainter* painter, const QPoint& pos, bool isSelf) const;
    void DrawTimestamp(QPainter* painter, const QRect& rect, const QString& timestamp) const;
    void DrawStatusIcon(QPainter* painter, const QPoint& pos, int status) const;

    mutable int _viewportWidth;
    mutable QMap<QString, QSize> _sizeCache;

    static const int AVATAR_SIZE;
    static const int BUBBLE_PADDING;
    static const int BUBBLE_RADIUS;
    static const int BUBBLE_MAX_WIDTH_RATIO;
    static const int VERTICAL_SPACING;
    static const int HORIZONTAL_MARGIN;
};

#endif
