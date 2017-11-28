#ifndef WEBFLOWCREDENTIALS_H
#define WEBFLOWCREDENTIALS_H

#include <QSslCertificate>
#include <QSslKey>

#include "creds/abstractcredentials.h"

class QNetworkReply;
class QAuthenticator;

namespace QKeychain {
    class Job;
}

namespace OCC {

class WebFlowCredentials : public AbstractCredentials
{
    Q_OBJECT
public:
    explicit WebFlowCredentials();
    WebFlowCredentials(const QString &user, const QString &password, const QSslCertificate &certificate = QSslCertificate(), const QSslKey &key = QSslKey());

    QString authType() const Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QString password() const;
    QNetworkAccessManager *createQNAM() const Q_DECL_OVERRIDE;

    bool ready() const Q_DECL_OVERRIDE;

    void fetchFromKeychain() Q_DECL_OVERRIDE;
    void askFromUser() Q_DECL_OVERRIDE;

    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void persist() Q_DECL_OVERRIDE;
    void invalidateToken() Q_DECL_OVERRIDE;
    void forgetSensitiveData() Q_DECL_OVERRIDE;

    // To fetch the user name as early as possible
    void setAccount(Account *account) Q_DECL_OVERRIDE;

private slots:
    void slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void slotFinished(QNetworkReply *reply);

    void slotReadPasswordJobDone(QKeychain::Job *incomingJob);

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
};

}

#endif // WEBFLOWCREDENTIALS_H
