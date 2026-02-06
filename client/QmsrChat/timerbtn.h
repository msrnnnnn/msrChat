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

    // 重写 mouseReleaseEvent 以拦截点击事件
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    QTimer *_timer;
    int _counter;
};

#endif // TIMERBTN_H
