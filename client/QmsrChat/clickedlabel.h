#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H

#include "global.h"
#include <QLabel>
#include <QWidget>

/**
 * @class ClickedLabel
 * @brief 可点击的标签控件
 * @details 继承自 QLabel，增加了点击事件响应和状态切换功能 (Normal/Selected)，
 * 并支持悬浮、点击等不同状态下的样式切换 (通过 QSS state 属性)。
 */
class ClickedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickedLabel(QWidget *parent = nullptr);

    /**
     * @brief 鼠标点击事件
     */
    void mousePressEvent(QMouseEvent *ev) override;

    /**
     * @brief 鼠标进入事件
     */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif

    /**
     * @brief 鼠标离开事件
     */
    void leaveEvent(QEvent *event) override;

    /**
     * @brief 设置各个状态下的 QSS 属性值
     * @param normal 普通状态属性值
     * @param hover 普通悬浮状态属性值
     * @param press 普通点击状态属性值 (暂未使用)
     * @param select 选中状态属性值
     * @param select_hover 选中悬浮状态属性值
     * @param select_press 选中点击状态属性值 (暂未使用)
     */
    void SetState(
        QString normal = "", QString hover = "", QString press = "", QString select = "", QString select_hover = "",
        QString select_press = "");

    /**
     * @brief 获取当前状态
     * @return ClickLbState
     */
    ClickLbState GetCurState();

signals:
    /**
     * @brief 点击信号
     */
    void clicked(void);

private:
    QString _normal;
    QString _normal_hover;
    QString _normal_press;
    QString _selected;
    QString _selected_hover;
    QString _selected_press;

    ClickLbState _curstate;
};

#endif // CLICKEDLABEL_H
