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
    explicit TimerBtn(QWidget *parent = nullptr);
    ~TimerBtn();

    void startCountdown(int seconds);
    void stopCountdown();
    void setAutoStart(bool autoStart);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void updateText();

    QTimer *_timer;
    int _counter;
    int _total;
    QString _defaultText;
    bool _autoStart;
};

#endif // TIMERBTN_H
