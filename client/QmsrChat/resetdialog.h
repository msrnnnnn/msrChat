/**
 * @file    resetdialog.h
 * @brief   重置密码对话框类
 * @details 负责用户重置密码的交互逻辑。
 */
#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include "global.h"
#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <functional>

namespace Ui
{
    class ResetDialog;
}

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ResetDialog(QWidget *parent = nullptr);
    /**
     * @brief 析构函数
     */
    ~ResetDialog();

signals:
    /**
     * @brief 返回登录界面信号
     */
    void switchLogin();

private slots:
    /**
     * @brief 获取验证码按钮点击槽
     */
    void on_varify_btn_clicked();
    /**
     * @brief 确认重置按钮点击槽
     */
    void on_sure_btn_clicked();
    /**
     * @brief TCP 回包处理槽
     * @param req_type 请求类型
     * @param data 响应数据
     */
    void slot_tcp_rsp(RequestType req_type, QByteArray data);

private:
    /**
     * @brief 校验用户名合法性
     * @return bool 是否有效
     */
    bool checkUserValid();
    /**
     * @brief 校验邮箱合法性
     * @return bool 是否有效
     */
    bool checkEmailValid();
    /**
     * @brief 校验密码合法性
     * @return bool 是否有效
     */
    bool checkPassValid();
    /**
     * @brief 校验验证码合法性
     * @return bool 是否有效
     */
    bool checkVarifyValid();
    /**
     * @brief 显示提示信息
     * @param str 提示文本
     * @param isCorrect 是否为成功提示
     */
    void showTip(QString str, bool isCorrect);
    /**
     * @brief 初始化回包处理器
     */
    void initHandlers();
    /**
     * @brief 记录错误提示
     * @param te 错误类型
     * @param tips 提示文本
     */
    void AddTipErr(TipErr te, QString tips);
    /**
     * @brief 移除错误提示
     * @param te 错误类型
     */
    void DelTipErr(TipErr te);

    Ui::ResetDialog *ui; ///< UI 指针
    QMap<RequestType, std::function<void(const QJsonObject &)>> _handlers; ///< 回包处理器
    QMap<TipErr, QString> _tip_errs; ///< 错误提示集合
};


#endif // RESETDIALOG_H
