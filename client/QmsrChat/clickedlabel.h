/**
 * @file    clickedlabel.h
 * @brief   自定义可点击标签类
 * @details 继承自 QLabel，增加了点击、悬停等状态处理。
 */
#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H

#include "global.h"
#include <QLabel>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif

class ClickedLabel : public QLabel
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ClickedLabel(QWidget *parent = nullptr);

    /**
     * @brief 设置各状态对应的样式名
     * @param normal 普通状态
     * @param hover 悬停状态
     * @param press 按下状态
     * @param select 选中状态
     * @param select_hover 选中悬停状态
     * @param select_press 选中按下状态
     */
    void SetState(QString normal = "", QString hover = "", QString press = "", QString select = "",
                  QString select_hover = "", QString select_press = "");
    /**
     * @brief 获取当前状态
     * @return ClickLbState 当前状态
     */
    ClickLbState GetCurState();

signals:
    /**
     * @brief 点击信号
     */
    void clicked();

protected:
    /**
     * @brief 鼠标按下事件
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;
    /**
     * @brief 鼠标释放事件
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /**
     * @brief 鼠标进入事件
     * @param event 进入事件
     */
    void enterEvent(QEnterEvent *event) override;
#else
    /**
     * @brief 鼠标进入事件
     * @param event 进入事件
     */
    void enterEvent(QEvent *event) override;
#endif
    /**
     * @brief 鼠标离开事件
     * @param event 离开事件
     */
    void leaveEvent(QEvent *event) override;

private:
    QString _normal;        ///< 普通状态样式名
    QString _normal_hover;  ///< 普通悬停样式名
    QString _normal_press;  ///< 普通按下样式名

    QString _selected;       ///< 选中状态样式名
    QString _selected_hover; ///< 选中悬停样式名
    QString _selected_press; ///< 选中按下样式名

    ClickLbState _curstate; ///< 当前状态
};

#endif // CLICKEDLABEL_H
