#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QColor>

/**
 * @brief 聊天气泡控件
 * @details 用于显示单条聊天消息，支持发送者和接收者两种样式
 */
class ChatBubble : public QWidget
{
    Q_OBJECT
public:
    enum UserRole {
        Sender,   // 发送者 (显示在右侧，绿色背景)
        Receiver  // 接收者 (显示在左侧，白色背景)
    };

    explicit ChatBubble(UserRole role, const QString &text, const QPixmap &avatar = QPixmap(), QWidget *parent = nullptr);

    /**
     * @brief 设置聊天内容
     * @param text 文本内容
     */
    void setText(const QString &text);

    /**
     * @brief 设置头像
     * @param avatar 头像图片
     */
    void setAvatar(const QPixmap &avatar);

    /**
     * @brief 设置角色
     * @param role 发送者或接收者
     */
    void setRole(UserRole role);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    UserRole m_role;
    QString m_text;
    QPixmap m_avatar;
    
    QLabel *m_pAvatarLabel; // 头像显示控件
    QLabel *m_pTextLabel;   // 文本显示控件
    QLabel *m_pNameLabel;   // 昵称显示控件 (可选，暂未详细实现)
    
    QHBoxLayout *m_pMainLayout;

    /**
     * @brief 生成默认头像
     * @details 当头像为空时，生成一个纯色圆形的默认头像
     * @return 默认头像 Pixmap
     */
    QPixmap generateDefaultAvatar();
};

#endif // CHATBUBBLE_H
