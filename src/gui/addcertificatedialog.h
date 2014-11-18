#ifndef ADDCERTIFICATEDIALOG_H
#define ADDCERTIFICATEDIALOG_H

#include <QDialog>
#include <QString>

namespace OCC {

namespace Ui {
class AddCertificateDialog;
}

class AddCertificateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddCertificateDialog(QWidget *parent = 0);
    ~AddCertificateDialog();
    QString getCertificatePath();
    QString getCertificatePasswd();
    void showErrorMessage(QString message);
    void Reinit();

private slots:
    void on_pushButtonBrowseCertificate_clicked();

private:
    Ui::AddCertificateDialog *ui;

};

}//End namespace OCC

#endif // ADDCERTIFICATEDIALOG_H
