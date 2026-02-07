#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QScrollArea>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "global.h"

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

protected:
    /**
     * @brief 事件过滤器
     * @details 用于监听输入框的键盘事件 (Enter 发送)
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /**
     * @brief 发送消息槽函数
     */
    void onSendBtnClicked();

    /**
     * @brief 消息列表滚动到底部
     */
    void scrollToBottom();

private:
    /**
     * @brief 初始化界面布局
     */
    void initUi();

    /**
     * @brief 添加一条测试消息 (用于演示)
     */
    void addChatBubble(bool isSender, const QString &text);

    // 界面控件
    QListWidget *m_pContactList;   // 左侧联系人列表
    
    // 右侧聊天区域控件
    QLabel *m_pTitleLabel;         // 聊天标题
    QScrollArea *m_pMsgScrollArea; // 消息滚动区域
    QWidget *m_pMsgContainer;      // 消息容器 (在滚动区域内)
    QVBoxLayout *m_pMsgLayout;     // 消息垂直布局
    
    QTextEdit *m_pInputEdit;       // 输入框
    QPushButton *m_pSendBtn;       // 发送按钮
};

#endif // CHATDIALOG_H
