/**
 * @file    timerbtn.h
 * @brief   倒计时按钮控件
 * @details 继承自 QPushButton，提供倒计时功能，常用于发送验证码按钮。
 */
#ifndef TIMERBTN_H
#define TIMERBTN_H

#include <QPushButton>
#include <QTimer>

class TimerBtn : public QPushButton
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit TimerBtn(QWidget *parent = nullptr);
    /**
     * @brief 析构函数
     */
    ~TimerBtn();

    /**
     * @brief 启动倒计时
     * @param seconds 倒计时秒数
     */
    void startCountdown(int seconds);
    /**
     * @brief 停止倒计时并恢复默认文本
     */
    void stopCountdown();
    /**
     * @brief 设置是否鼠标释放后自动启动倒计时
     * @param autoStart 是否自动启动
     */
    void setAutoStart(bool autoStart);

protected:
    /**
     * @brief 鼠标释放事件
     * @param e 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    /**
     * @brief 更新按钮显示文本
     */
    void updateText();

    QTimer *_timer;       ///< 倒计时定时器
    int _counter;         ///< 当前剩余秒数
    int _total;           ///< 初始倒计时秒数
    QString _defaultText; ///< 默认显示文本
    bool _autoStart;      ///< 是否自动启动倒计时
};

#endif // TIMERBTN_H
