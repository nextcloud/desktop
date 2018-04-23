#ifndef WEBFLOWCREDENTIALS_H
#define WEBFLOWCREDENTIALS_H

#include <QSslCertificate>
#include <QSslKey>

#include "creds/abstractcredentials.h"

class QDialog;
class QLabel;
class QNetworkReply;
class QAuthenticator;

namespace QKeychain {
    class Job;
}

namespace OCC {

class WebFlowCredentialsDialog;

class WebFlowCredentials : public AbstractCredentials
{
    Q_OBJECT
public:
    explicit WebFlowCredentials();
    WebFlowCredentials(const QString &user, const QString &password, const QSslCertificate &certificate = QSslCertificate(), const QSslKey &key = QSslKey());

    QString authType() const override;
    QString user() const override;
    QString password() const;
    QNetworkAccessManager *createQNAM() const override;

    bool ready() const override;

    void fetchFromKeychain() override;
    void askFromUser() override;

    bool stillValid(QNetworkReply *reply) override;
    void persist() override;
    void invalidateToken() override;
    void forgetSensitiveData() override;

    // To fetch the user name as early as possible
    void setAccount(Account *account) override;

private slots:
    void slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void slotFinished(QNetworkReply *reply);

    void slotReadPasswordJobDone(QKeychain::Job *incomingJob);
    void slotAskFromUserCredentialsProvided(const QString &user, const QString &pass, const QString &host);

private:
    /** Reads data from keychain locations
     *
     * Goes through
     *   slotReadClientCertPEMJobDone to
     *   slotReadClientCertPEMJobDone to
     *   slotReadJobDone
     */
    void fetchFromKeychainHelper();

    QString fetchUser();

    QString _user;
    QString _password;
    QSslKey _clientSslKey;
    QSslCertificate _clientSslCertificate;

    bool _ready;
    bool _credentialsValid;

    WebFlowCredentialsDialog *_askDialog;
};

}

#endif // WEBFLOWCREDENTIALS_H
