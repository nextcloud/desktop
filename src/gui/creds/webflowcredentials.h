#ifndef WEBFLOWCREDENTIALS_H
#define WEBFLOWCREDENTIALS_H

#include <QSslCertificate>
#include <QSslKey>
#include <QNetworkRequest>

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
    friend class WebFlowCredentialsAccessManager;

public:
    /// Don't add credentials if this is set on a QNetworkRequest
    static constexpr QNetworkRequest::Attribute DontAddCredentialsAttribute = QNetworkRequest::User;

    explicit WebFlowCredentials();
    WebFlowCredentials(
            const QString &user,
            const QString &password,
            const QSslCertificate &certificate = QSslCertificate(),
            const QSslKey &key = QSslKey(),
            const QList<QSslCertificate> &caCertificates = QList<QSslCertificate>());

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

    void slotAskFromUserCredentialsProvided(const QString &user, const QString &pass, const QString &host);

    void slotReadClientCertPEMJobDone(QKeychain::Job *incomingJob);
    void slotReadClientKeyPEMJobDone(QKeychain::Job *incomingJob);
    void slotReadClientCaCertsPEMJobDone(QKeychain::Job *incommingJob);
    void slotReadPasswordJobDone(QKeychain::Job *incomingJob);

    void slotWriteClientCertPEMJobDone();
    void slotWriteClientKeyPEMJobDone();
    void slotWriteClientCaCertsPEMJobDone();
    void slotWriteJobDone(QKeychain::Job *);

protected:
    /** Reads data from keychain locations
     *
     * Goes through
     *   slotReadClientCertPEMJobDone to
     *   slotReadClientKeyPEMJobDone to
     *   slotReadClientCaCertsPEMJobDone to
     *   slotReadJobDone
     */
    void fetchFromKeychainHelper();

    /// Wipes legacy keychain locations
    void deleteOldKeychainEntries();

    QString fetchUser();

    QString _user;
    QString _password;
    QSslKey _clientSslKey;
    QSslCertificate _clientSslCertificate;
    QList<QSslCertificate> _clientSslCaCertificates;

    bool _ready;
    bool _credentialsValid;
    bool _keychainMigration;
    bool _retryOnKeyChainError = true; // true if we haven't done yet any reading from keychain

    WebFlowCredentialsDialog *_askDialog;
};

}

#endif // WEBFLOWCREDENTIALS_H
