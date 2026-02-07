#include "chatbubble.h"
#include <QFontMetrics>

ChatBubble::ChatBubble(UserRole role, const QString &text, const QPixmap &avatar, QWidget *parent)
    : QWidget(parent), m_role(role), m_text(text), m_avatar(avatar)
{
    // 设置气泡的布局
    m_pMainLayout = new QHBoxLayout(this);
    m_pMainLayout->setContentsMargins(10, 10, 10, 10);
    m_pMainLayout->setSpacing(10);

    // 初始化控件
    m_pAvatarLabel = new QLabel(this);
    m_pAvatarLabel->setFixedSize(40, 40);
    m_pAvatarLabel->setScaledContents(true);

    m_pTextLabel = new QLabel(this);
    m_pTextLabel->setWordWrap(true);
    m_pTextLabel->setMaximumWidth(parent ? parent->width() * 0.6 : 300); // 限制最大宽度
    m_pTextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse); // 允许复制文本
    QFont font("Microsoft YaHei", 10);
    m_pTextLabel->setFont(font);

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
    // 根据文本内容调整 label 大小（虽然布局会自动处理，但有时需要强制刷新）
    m_pTextLabel->adjustSize();
}

void ChatBubble::setAvatar(const QPixmap &avatar)
{
    if (avatar.isNull()) {
        m_avatar = generateDefaultAvatar();
    } else {
        m_avatar = avatar;
    }
    m_pAvatarLabel->setPixmap(m_avatar);
}

void ChatBubble::setRole(UserRole role)
{
    m_role = role;
    
    // 清除现有布局项
    QLayoutItem *item;
    while ((item = m_pMainLayout->takeAt(0)) != nullptr) {
        // 不删除 widget，只是从布局中移除
        delete item;
    }

    // 重新添加控件
    if (m_role == Receiver) {
        // 接收者：头像在左，文本在右
        m_pMainLayout->addWidget(m_pAvatarLabel, 0, Qt::AlignTop);
        m_pMainLayout->addWidget(m_pTextLabel, 1);
        m_pMainLayout->addStretch(1); // 右侧填充
    } else {
        // 发送者：文本在左，头像在右
        m_pMainLayout->addStretch(1); // 左侧填充
        m_pMainLayout->addWidget(m_pTextLabel, 1);
        m_pMainLayout->addWidget(m_pAvatarLabel, 0, Qt::AlignTop);
    }
}

QPixmap ChatBubble::generateDefaultAvatar()
{
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制灰色圆形背景
    painter.setBrush(QColor(200, 200, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 40, 40);
    
    // 绘制文字（首字母）
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "U"); // 默认为 User 的 U
    
    return pixmap;
}

void ChatBubble::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 获取文本控件的几何区域
    // 我们需要在文本控件周围绘制气泡背景
    // 稍微扩大一点区域作为气泡边界
    QRect textRect = m_pTextLabel->geometry();
    QRect bubbleRect = textRect.adjusted(-10, -5, 10, 5);

    QColor bubbleColor;
    if (m_role == Sender) {
        bubbleColor = QColor("#95ec69"); // 微信绿
    } else {
        bubbleColor = QColor("#ffffff"); // 白色
    }

    painter.setBrush(bubbleColor);
    painter.setPen(Qt::NoPen);

    // 绘制圆角矩形气泡
    painter.drawRoundedRect(bubbleRect, 5, 5);

    // 绘制小三角 (可选，增加气泡感)
    QPainterPath path;
    if (m_role == Sender) {
        // 右侧小三角
        QPointF topRight(bubbleRect.right(), bubbleRect.top() + 15);
        QPointF bottomRight(bubbleRect.right(), bubbleRect.top() + 25);
        QPointF tip(bubbleRect.right() + 6, bubbleRect.top() + 20);
        path.moveTo(topRight);
        path.lineTo(tip);
        path.lineTo(bottomRight);
        path.closeSubpath();
    } else {
        // 左侧小三角
        QPointF topLeft(bubbleRect.left(), bubbleRect.top() + 15);
        QPointF bottomLeft(bubbleRect.left(), bubbleRect.top() + 25);
        QPointF tip(bubbleRect.left() - 6, bubbleRect.top() + 20);
        path.moveTo(topLeft);
        path.lineTo(tip);
        path.lineTo(bottomLeft);
        path.closeSubpath();
    }
    painter.drawPath(path);
}
