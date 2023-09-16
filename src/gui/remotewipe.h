#ifndef REMOTEWIPE_H
#define REMOTEWIPE_H

#include "accountmanager.h"
#include <QNetworkAccessManager>

class QJsonDocument;
class TestRemoteWipe;

namespace OCC {

class RemoteWipe : public QObject
{
    Q_OBJECT
public:
    explicit RemoteWipe(AccountPtr account, QObject *parent = nullptr);

signals:
    /**
     * Notify if wipe was requested
     */
    void authorized(OCC::AccountState*);

    /**
     * Notify if user only needs to login again
     */
    void askUserCredentials();

public slots:
    /**
     * Once receives a 401 or 403 status response it will do a fetch to
     * <server>/index.php/core/wipe/check
     */
    void startCheckJobWithAppPassword(QString);

private slots:
    /**
     * If wipe is requested, delete account and data, if not continue by asking
     * the user to login again
     */
    void checkJobSlot();

    /**
     * Once the client has wiped all the required data a POST to
     * <server>/index.php/core/wipe/success
     */
    void notifyServerSuccessJob(OCC::AccountState *accountState, bool);
    void notifyServerSuccessJobSlot();

private:
    AccountPtr _account;
    QString _appPassword;
    bool _accountRemoved = false;
    QNetworkAccessManager _networkManager;
    QNetworkReply *_networkReplyCheck = nullptr;
    QNetworkReply *_networkReplySuccess = nullptr;

    friend class ::TestRemoteWipe;
};
}
#endif // REMOTEWIPE_H
