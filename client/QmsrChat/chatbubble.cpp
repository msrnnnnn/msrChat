#include "chatbubble.h"
#include <QFontMetrics>

ChatBubble::ChatBubble(UserRole role, const QString &text, const QPixmap &avatar, QWidget *parent)
    : QWidget(parent),
      m_role(role),
      m_text(text),
      m_avatar(avatar)
{
    // 设置气泡的布局
    m_pMainLayout = new QHBoxLayout(this);
    m_pMainLayout->setContentsMargins(10, 5, 10, 5); // 减小垂直边距
    m_pMainLayout->setSpacing(5);

    // 初始化控件
    m_pAvatarLabel = new QLabel(this);
    m_pAvatarLabel->setFixedSize(35, 35); // 稍微调小头像
    m_pAvatarLabel->setScaledContents(true);

    m_pTextLabel = new QLabel(this);
    m_pTextLabel->setWordWrap(true);
    m_pTextLabel->setMaximumWidth(parent ? parent->width() * 0.6 : 400); // 限制最大宽度 400px
    m_pTextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont font("Microsoft YaHei", 10);
    m_pTextLabel->setFont(font);

    // 设置内边距，为了让文字不贴着气泡边缘
    m_pTextLabel->setContentsMargins(10, 8, 10, 8);

    // 设置头像和文本
    setAvatar(m_avatar);
    setText(m_text);

    // 根据角色调整布局方向
    setRole(m_role);
}

void ChatBubble::setText(const QString &text)
{
    m_text = text;
    m_pTextLabel->setText(m_text);
    m_pTextLabel->adjustSize();
}

void ChatBubble::setAvatar(const QPixmap &avatar)
{
    if (avatar.isNull())
    {
        m_avatar = generateDefaultAvatar();
    }
    else
    {
        m_avatar = avatar;
    }
    m_pAvatarLabel->setPixmap(m_avatar);
}

void ChatBubble::setRole(UserRole role)
{
    m_role = role;

    QLayoutItem *item;
    while ((item = m_pMainLayout->takeAt(0)) != nullptr)
    {
        delete item;
    }

    if (m_role == Receiver)
    {
        // 接收者：头像在左，文本在右
        m_pMainLayout->addWidget(m_pAvatarLabel, 0, Qt::AlignTop);
        m_pMainLayout->addWidget(m_pTextLabel, 1);
        m_pMainLayout->addStretch(1);
    }
    else
    {
        // 发送者：文本在左，头像在右
        m_pMainLayout->addStretch(1);
        m_pMainLayout->addWidget(m_pTextLabel, 1);
        m_pMainLayout->addWidget(m_pAvatarLabel, 0, Qt::AlignTop);
    }
}

QPixmap ChatBubble::generateDefaultAvatar()
{
    QPixmap pixmap(35, 35);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(QColor(200, 200, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 35, 35);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "U");

    return pixmap;
}

void ChatBubble::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect textRect = m_pTextLabel->geometry();
    // 气泡紧贴 TextLabel
    QRect bubbleRect = textRect;

    QColor bubbleColor;
    if (m_role == Sender)
    {
        bubbleColor = QColor("#95ec69"); // 微信绿
    }
    else
    {
        bubbleColor = QColor("#ffffff"); // 白色
    }

    painter.setBrush(bubbleColor);
    painter.setPen(Qt::NoPen);

    // 绘制圆角矩形
    if (m_role == Sender)
    {
        // 发送者：左侧圆角，右侧直角 (根据需求微调，这里使用通用圆角)
        painter.drawRoundedRect(bubbleRect, 5, 5);
    }
    else
    {
        painter.drawRoundedRect(bubbleRect, 5, 5);
    }

    // 绘制小三角
    QPainterPath path;
    if (m_role == Sender)
    {
        QPointF topRight(bubbleRect.right(), bubbleRect.top() + 12);
        QPointF bottomRight(bubbleRect.right(), bubbleRect.top() + 22);
        QPointF tip(bubbleRect.right() + 6, bubbleRect.top() + 17);
        path.moveTo(topRight);
        path.lineTo(tip);
        path.lineTo(bottomRight);
        path.closeSubpath();
    }
    else
    {
        QPointF topLeft(bubbleRect.left(), bubbleRect.top() + 12);
        QPointF bottomLeft(bubbleRect.left(), bubbleRect.top() + 22);
        QPointF tip(bubbleRect.left() - 6, bubbleRect.top() + 17);
        path.moveTo(topLeft);
        path.lineTo(tip);
        path.lineTo(bottomLeft);
        path.closeSubpath();
    }
    painter.drawPath(path);
}
