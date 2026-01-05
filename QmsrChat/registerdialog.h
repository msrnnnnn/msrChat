#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    void switchLogin();

private slots:
    void on_confirm_CAPTCHA_Button_clicked();

private:
    void showTip(QString str, bool isCorrect);
    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
