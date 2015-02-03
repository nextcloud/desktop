#ifndef OWNCLOUDCONNECTIONMETHODDIALOG_H
#define OWNCLOUDCONNECTIONMETHODDIALOG_H

#include <QDialog>

#include "ui_owncloudconnectionmethoddialog.h"

namespace Ui {
class OwncloudConnectionMethodDialog;
}

class OwncloudConnectionMethodDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OwncloudConnectionMethodDialog(QWidget *parent = 0);
    ~OwncloudConnectionMethodDialog();
    enum {
        No_TLS,
        Client_Side_TLS,
        Back
    };
    
    // The URL that was tried
    void setUrl(const QUrl &);

public slots:
    void returnNoTLS();
    void returnClientSideTLS();
    void returnBack();

private:
    Ui::OwncloudConnectionMethodDialog *ui;
};

#endif // OWNCLOUDCONNECTIONMETHODDIALOG_H
