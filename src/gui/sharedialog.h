#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include "networkjobs.h"
#include <QDialog>

namespace Mirall {

class OcsShareJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit OcsShareJob(const QByteArray &verb, const QUrl &url, const QUrl &postData, Account *account, QObject* parent = 0);
public slots:
    void start() Q_DECL_OVERRIDE;
signals:
    void jobFinished(QString reply);
private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
private:
    QByteArray _verb;
    QUrl _url;
    QUrl _postData;
};

namespace Ui {
class ShareDialog;
}

class AbstractCredentials;
class Account;
class QuotaInfo;
class MirallAccessManager;

class ShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareDialog(QWidget *parent = 0);
    ~ShareDialog();
    void getShares();
    void setPath(const QString &path);
    QString getPath();
private slots:
    void slotSharesFetched(QString reply);
    void slotCreateShareFetched(QString reply);
    void slotDeleteShareFetched(QString reply);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
private:
    Ui::ShareDialog *_ui;
    QString _path;
    QList<QVariant> _shares;
    QVariantMap _public_share;
};

}

#endif // SHAREDIALOG_H
