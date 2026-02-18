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
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

signals:
    void switchLogin();

private slots:
    void on_varify_btn_clicked();
    void on_sure_btn_clicked();
    void slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod);

private:
    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkVarifyValid();
    void showTip(QString str, bool isCorrect);
    void initHandlers();

    Ui::ResetDialog *ui;
    QMap<RequestType, std::function<void(const QJsonObject &)>> _handlers;
};

#endif // RESETDIALOG_H
