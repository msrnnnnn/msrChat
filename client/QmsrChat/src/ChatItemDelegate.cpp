#include "ChatItemDelegate.h"
#include <QPainter>
#include <QTextOption>
#include <QFontMetrics>
#include <QDebug>

const int ChatItemDelegate::AVATAR_SIZE = 40;
const int ChatItemDelegate::BUBBLE_PADDING = 10;
const int ChatItemDelegate::BUBBLE_RADIUS = 8;
const int ChatItemDelegate::BUBBLE_MAX_WIDTH_RATIO = 70;
const int ChatItemDelegate::VERTICAL_SPACING = 5;
const int ChatItemDelegate::HORIZONTAL_MARGIN = 10;

ChatItemDelegate::ChatItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , _viewportWidth(500)
{
}

ChatItemDelegate::~ChatItemDelegate()
{
}

void ChatItemDelegate::SetViewportWidth(int width)
{
    if (_viewportWidth != width) {
        _viewportWidth = width;
        _sizeCache.clear();
    }
}

QSize ChatItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QSize(_viewportWidth, 60);
    }

    QString content = index.data(Qt::UserRole + 2).toString();
    bool isSelf = index.data(Qt::UserRole + 6).toBool();

    QString cacheKey = QString("%1_%2_%3").arg(content).arg(isSelf).arg(_viewportWidth);
    
    if (_sizeCache.contains(cacheKey)) {
        return _sizeCache[cacheKey];
    }

    QSize bubbleSize = CalculateBubbleSize(content, isSelf, _viewportWidth);
    
    int totalHeight = bubbleSize.height() + VERTICAL_SPACING * 2 + AVATAR_SIZE + VERTICAL_SPACING;
    
    QSize result(_viewportWidth, totalHeight);
    _sizeCache[cacheKey] = result;
    
    return result;
}

void ChatItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QString content = index.data(Qt::UserRole + 2).toString();
    QString timestamp = index.data(Qt::UserRole + 7).toString();
    bool isSelf = index.data(Qt::UserRole + 6).toBool();
    int status = index.data(Qt::UserRole + 5).toInt();

    QRect itemRect = option.rect;
    int contentWidth = itemRect.width();

    int bubbleMaxWidth = static_cast<int>(contentWidth * BUBBLE_MAX_WIDTH_RATIO / 100.0);
    QSize bubbleSize = CalculateBubbleSize(content, isSelf, bubbleMaxWidth);
    
    int avatarY = itemRect.top() + VERTICAL_SPACING;
    int bubbleY = avatarY;
    
    QRect avatarRect;
    QRect bubbleRect;

    if (isSelf) {
        avatarRect = QRect(itemRect.right() - HORIZONTAL_MARGIN - AVATAR_SIZE, avatarY, AVATAR_SIZE, AVATAR_SIZE);
        bubbleRect = QRect(avatarRect.left() - bubbleSize.width() - HORIZONTAL_MARGIN, bubbleY, bubbleSize.width(), bubbleSize.height());
    } else {
        avatarRect = QRect(itemRect.left() + HORIZONTAL_MARGIN, avatarY, AVATAR_SIZE, AVATAR_SIZE);
        bubbleRect = QRect(avatarRect.right() + HORIZONTAL_MARGIN, bubbleY, bubbleSize.width(), bubbleSize.height());
    }

    DrawAvatar(painter, avatarRect.topLeft(), isSelf);
    DrawBubble(painter, bubbleRect, isSelf);
    
    QRect textRect = bubbleRect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING, -BUBBLE_PADDING, -BUBBLE_PADDING);
    DrawText(painter, textRect, content, isSelf);

    if (isSelf) {
        QPoint statusPos(avatarRect.left(), avatarRect.bottom() + 2);
        DrawStatusIcon(painter, statusPos, status);
    }

    painter->restore();
}

QSize ChatItemDelegate::CalculateBubbleSize(const QString& content, bool isSelf, int viewportWidth) const
{
    if (content.isEmpty()) {
        return QSize(50, 30);
    }

    int maxWidth = static_cast<int>(viewportWidth * BUBBLE_MAX_WIDTH_RATIO / 100.0);
    int bubbleContentWidth = maxWidth - BUBBLE_PADDING * 2;

    QFont font("Microsoft YaHei", 12);
    QFontMetrics fm(font);
    
    QString processedContent = content;
    processedContent.replace("\n", " ");
    
    int textWidth = fm.horizontalAdvance(processedContent);
    
    if (textWidth > bubbleContentWidth) {
        int charsPerLine = bubbleContentWidth / fm.averageCharWidth();
        int lines = (textWidth + bubbleContentWidth - 1) / bubbleContentWidth;
        
        QString wrapped;
        int currentLineWidth = 0;
        int charCount = 0;
        
        for (int i = 0; i < processedContent.length(); ++i) {
            QChar ch = processedContent[i];
            int charWidth = fm.horizontalAdvance(ch);
            
            if (currentLineWidth + charWidth > bubbleContentWidth && charCount > 0) {
                wrapped += "\n";
                currentLineWidth = charWidth;
            } else {
                currentLineWidth += charWidth;
            }
            
            wrapped += ch;
            charCount++;
        }
        
        processedContent = wrapped;
        textWidth = bubbleContentWidth;
    }

    int bubbleWidth = qMin(textWidth + BUBBLE_PADDING * 2, maxWidth);
    int bubbleHeight = fm.height() + BUBBLE_PADDING * 2;

    if (processedContent.contains('\n')) {
        QStringList lines = processedContent.split('\n');
        int maxLineWidth = 0;
        for (const QString& line : lines) {
            int lineWidth = fm.horizontalAdvance(line);
            if (lineWidth > maxLineWidth) {
                maxLineWidth = lineWidth;
            }
        }
        bubbleWidth = qMin(maxLineWidth + BUBBLE_PADDING * 2, maxWidth);
        bubbleHeight = lines.size() * fm.height() + BUBBLE_PADDING * 2;
    }

    bubbleWidth = qMax(bubbleWidth, 50);
    bubbleHeight = qMax(bubbleHeight, 30);

    return QSize(bubbleWidth, bubbleHeight);
}

void ChatItemDelegate::DrawBubble(QPainter* painter, const QRect& rect, bool isSelf) const
{
    QPainterPath path;
    path.addRoundedRect(rect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    if (isSelf) {
        painter->setPen(QColor("#95EC69"));
        painter->setBrush(QColor("#95EC69"));
    } else {
        painter->setPen(QColor("#FFFFFF"));
        painter->setBrush(QColor("#FFFFFF"));
    }

    painter->drawPath(path);

    painter->setPen(QColor(200, 200, 200));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

void ChatItemDelegate::DrawText(QPainter* painter, const QRect& rect, const QString& text, bool isSelf) const
{
    QFont font("Microsoft YaHei", 12);
    painter->setFont(font);
    
    if (isSelf) {
        painter->setPen(QColor("#000000"));
    } else {
        painter->setPen(QColor("#333333"));
    }

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    painter->drawText(rect, text, textOption);
}

void ChatItemDelegate::DrawAvatar(QPainter* painter, const QPoint& pos, bool isSelf) const
{
    QRect avatarRect(pos.x(), pos.y(), AVATAR_SIZE, AVATAR_SIZE);
    
    QPainterPath path;
    path.addEllipse(avatarRect);
    
    painter->setClipPath(path);

    if (isSelf) {
        painter->setBrush(QColor("#07C160"));
    } else {
        painter->setBrush(QColor("#576B95"));
    }
    painter->setPen(Qt::NoPen);
    painter->drawRect(avatarRect);

    QFont font("Microsoft YaHei", 16, QFont::Bold);
    painter->setFont(font);
    painter->setPen(Qt::white);
    
    QString initial = isSelf ? "W" : "U";
    QRect textRect = avatarRect;
    painter->drawText(textRect, Qt::AlignCenter, initial);

    painter->setClipping(false);
}

void ChatItemDelegate::DrawTimestamp(QPainter* painter, const QRect& rect, const QString& timestamp) const
{
    QFont font("Microsoft YaHei", 10);
    painter->setFont(font);
    painter->setPen(QColor(150, 150, 150));
    
    painter->drawText(rect, Qt::AlignCenter, timestamp);
}

void ChatItemDelegate::DrawStatusIcon(QPainter* painter, const QPoint& pos, int status) const
{
    QFont font("Microsoft YaHei", 10);
    painter->setFont(font);
    painter->setPen(QColor(100, 100, 100));
    
    QString statusText;
    switch (status) {
        case 0:
            statusText = "发送中...";
            break;
        case 1:
            statusText = "已发送";
            painter->setPen(QColor(7, 192, 96));
            break;
        case 2:
            statusText = "已读";
            painter->setPen(QColor(7, 192, 96));
            break;
        default:
            statusText = "失败";
            painter->setPen(QColor(255, 0, 0));
            break;
    }
    
    painter->drawText(pos.x(), pos.y() + 10, statusText);
}
