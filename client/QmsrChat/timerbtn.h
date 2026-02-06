/**
 * @file timerbtn.h
 * @brief 倒计时按钮控件
 * @author msr
 */

#ifndef TIMERBTN_H
#define TIMERBTN_H

#include <QPushButton>
#include <QTimer>

/**
 * @class TimerBtn
 * @brief 带有倒计时功能的按钮
 * @details 点击后可触发倒计时，倒计时结束前禁用按钮，常用于获取验证码场景。
 */
class TimerBtn : public QPushButton
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit TimerBtn(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TimerBtn();

    /**
     * @brief 重写鼠标释放事件
     * @param e 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *e) override;

    /**
     * @brief 停止倒计时并恢复按钮状态
     */
    void Stop();

private:
    QTimer *_timer; ///< 定时器
    int _counter;   ///< 计数器
};

#endif // TIMERBTN_H
