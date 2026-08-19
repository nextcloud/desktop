/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WEBFLOWCREDENTIALS_H
#define WEBFLOWCREDENTIALS_H

#include <QSslCertificate>
#include <QSslKey>
#include <QNetworkRequest>
#include <QQueue>
#include <QPointer>

#include "creds/abstractcredentials.h"

class QNetworkReply;
class QAuthenticator;

namespace QKeychain {
    class Job;
}

namespace OCC {

namespace KeychainChunk {
    class ReadJob;
    class WriteJob;
}

class BrowserReAuthWindow;

class WebFlowCredentials : public AbstractCredentials
{
    Q_OBJECT
    friend class WebFlowCredentialsAccessManager;

public:
    explicit WebFlowCredentials();
    WebFlowCredentials(
            const QString &user,
            const QString &password,
            const QSslCertificate &certificate = QSslCertificate(),
            const QSslKey &key = QSslKey(),
            const QList<QSslCertificate> &caCertificates = QList<QSslCertificate>());

    [[nodiscard]] QString authType() const override;
    [[nodiscard]] QString user() const override;
    [[nodiscard]] QString password() const override;
    [[nodiscard]] QNetworkAccessManager *createQNAM() const override;

    [[nodiscard]] bool ready() const override;

    void fetchFromKeychain(const QString &appName = {}) override;
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

    void slotAskFromUserCredentialsProvided(const QString &user, const QString &pass);
    void slotAskFromUserCancelled();

    void slotReadClientCertPEMJobDone(OCC::KeychainChunk::ReadJob *readJob);
    void slotReadClientKeyPEMJobDone(OCC::KeychainChunk::ReadJob *readJob);
    void slotReadClientCaCertsPEMJobDone(OCC::KeychainChunk::ReadJob *readJob);
    void slotReadPasswordJobDone(QKeychain::Job *incomingJob);

    void slotWriteClientCertPEMJobDone(OCC::KeychainChunk::WriteJob *writeJob);
    void slotWriteClientKeyPEMJobDone(OCC::KeychainChunk::WriteJob *writeJob);
    void slotWriteClientCaCertsPEMJobDone(OCC::KeychainChunk::WriteJob *writeJob);
    void slotWriteJobDone(QKeychain::Job *);

private:
    /*
     * Windows: Workaround for CredWriteW used by QtKeychain
     *
     *          Saving all client CA's within one credential may result in:
     *          Error: "Credential size exceeds maximum size of 2560"
     */
    void readSingleClientCaCertPEM();
    void writeSingleClientCaCertPEM();

    /** Confirms that freshly provided browser credentials still belong to the
     * account's persistent dav_user before they are persisted.
     *
     * The dav_user is the unique, persistent account id and must stay constant.
     * The webflow_user (login name) may legitimately differ from it and change
     * between logins, so it is updated in the configuration while dav_user is
     * kept. Fetches ocs/.../cloud/user with the new credentials and, on success,
     * either persists them (same or first-seen dav_user) or rejects and re-asks
     * (different dav_user).
     *
     * @param previousDavUser the account's stored dav_user, empty if never set.
     * @param previousUser the webflow_user in use before this re-authentication.
     */
    void verifyReAuthenticatedUser(const QString &previousDavUser, const QString &previousUser);

    /*
     * Since we're limited by Windows limits, we just create our own
     * limit to avoid evil things happening by endless recursion
     *
     * Better than storing the count and relying on maybe-hacked values
     */
    static constexpr int _clientSslCaCertificatesMaxCount = 10;
    QQueue<QSslCertificate> _clientSslCaCertificatesWriteQueue;

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
    void deleteKeychainEntries(bool oldKeychainEntries = false);

    QString fetchUser();

    QString _user;
    QString _password;
    QSslKey _clientSslKey;
    QSslCertificate _clientSslCertificate;
    QList<QSslCertificate> _clientSslCaCertificates;

    bool _ready = false;
    bool _credentialsValid = false;
    bool _keychainMigration = false;
    QString _appName;

    QPointer<BrowserReAuthWindow> _reAuthWindow;
};

} // namespace OCC

#endif // WEBFLOWCREDENTIALS_H
