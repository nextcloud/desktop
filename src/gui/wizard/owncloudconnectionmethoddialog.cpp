#include "wizard/owncloudconnectionmethoddialog.h"

OwncloudConnectionMethodDialog::OwncloudConnectionMethodDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OwncloudConnectionMethodDialog)
{
    ui->setupUi(this);

    connect(ui->btnNoTLS, SIGNAL(clicked(bool)), this, SLOT(returnNoTLS()));
    connect(ui->btnClientSideTLS, SIGNAL(clicked(bool)), this, SLOT(returnClientSideTLS()));
    connect(ui->btnBack, SIGNAL(clicked(bool)), this, SLOT(returnBack()));
}

void OwncloudConnectionMethodDialog::returnNoTLS()
{
    done(No_TLS);
}

void OwncloudConnectionMethodDialog::returnClientSideTLS()
{
    done(Client_Side_TLS);
}

void OwncloudConnectionMethodDialog::returnBack()
{
    done(Back);
}

OwncloudConnectionMethodDialog::~OwncloudConnectionMethodDialog()
{
    delete ui;
}
