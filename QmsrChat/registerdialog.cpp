#include "registerdialog.h"
#include "ui_registerdialog.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}
