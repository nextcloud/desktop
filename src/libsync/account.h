/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#ifndef SERVERCONNECTION_H
#define SERVERCONNECTION_H

#include <QByteArray>
#include <QUrl>
#include <QNetworkCookie>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QSslError>
#include <QSharedPointer>

#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#endif

#include "capabilities.h"
#include "clientsideencryption.h"
#include "clientstatusreporting.h"
#include "common/utility.h"
#include "syncfileitem.h"

#include <memory>

class QSettings;
class QNetworkReply;
class QUrl;
class QNetworkAccessManager;

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

class AbstractCredentials;
class Account;
using AccountPtr = QSharedPointer<Account>;
class AccessManager;
class SimpleNetworkJob;
class PushNotifications;
class UserStatusConnector;
class SyncJournalDb;

/**
 * @brief Reimplement this to handle SSL errors from libsync
 * @ingroup libsync
 */
class AbstractSslErrorHandler
{
public:
    virtual ~AbstractSslErrorHandler() = default;
    virtual bool handleErrors(QList<QSslError>, const QSslConfiguration &conf, QList<QSslCertificate> *, AccountPtr) = 0;
};

/**
 * @brief The Account class represents an account on an ownCloud Server
 * @ingroup libsync
 *
 * The Account has a name and url. It also has information about credentials,
 * SSL errors and certificates.
 */
class OWNCLOUDSYNC_EXPORT Account : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id MEMBER _id)
    Q_PROPERTY(QString davUser MEMBER _davUser)
    Q_PROPERTY(QString displayName MEMBER _displayName)
    Q_PROPERTY(QString prettyName READ prettyName NOTIFY prettyNameChanged)
    Q_PROPERTY(QUrl url MEMBER _url)
    Q_PROPERTY(bool e2eEncryptionKeysGenerationAllowed MEMBER _e2eEncryptionKeysGenerationAllowed)
    Q_PROPERTY(bool askUserForMnemonic READ askUserForMnemonic WRITE setAskUserForMnemonic NOTIFY askUserForMnemonicChanged)

public:
    static AccountPtr create();
    ~Account() override;

    AccountPtr sharedFromThis();

    [[nodiscard]] AccountPtr sharedFromThis() const;

    /**
     * The user that can be used in dav url.
     *
     * This can very well be different from the login user that's
     * stored in credentials()->user().
     */
    [[nodiscard]] QString davUser() const;
    void setDavUser(const QString &newDavUser);

    [[nodiscard]] QString davDisplayName() const;
    void setDavDisplayName(const QString &newDisplayName);

#ifndef TOKEN_AUTH_ONLY
    [[nodiscard]] QImage avatar() const;
    void setAvatar(const QImage &img);
#endif

    /// The name of the account as shown in the toolbar
    [[nodiscard]] QString displayName() const;

    /// User id in a form 'user@example.de, optionally port is added (if it is not 80 or 443)
    [[nodiscard]] QString userIdAtHostWithPort() const;

    /// The name of the account that is displayed as nicely as possible,
    /// e.g. the actual name of the user (John Doe). If this cannot be
    /// provided, defaults to davUser (e.g. johndoe)
    [[nodiscard]] QString prettyName() const;

    [[nodiscard]] QColor accentColor() const;
    [[nodiscard]] QColor headerColor() const;
    [[nodiscard]] QColor headerTextColor() const;

    /// The internal id of the account.
    [[nodiscard]] QString id() const;

    /** Server url of the account */
    void setUrl(const QUrl &url);
    [[nodiscard]] QUrl url() const { return _url; }

    /// Adjusts _userVisibleUrl once the host to use is discovered.
    void setUserVisibleHost(const QString &host);

    /**
     * @brief The possibly themed dav path for the account. It has
     *        a trailing slash.
     * @returns the (themeable) dav path for the account.
     */
    [[nodiscard]] QString davPath() const;

    /**
     * @brief The possibly themed dav path root for the account. It has
     *        no trailing slash.
     * @returns the (themeable) dav path for the account.
     */
    [[nodiscard]] QString davPathRoot() const;

    /** Returns webdav entry URL, based on url() */
    [[nodiscard]] QUrl davUrl() const;

    /** Returns the legacy permalink url for a file.
     *
     * This uses the old way of manually building the url. New code should
     * use the "privatelink" property accessible via PROPFIND.
     */
    [[nodiscard]] QUrl deprecatedPrivateLinkUrl(const QByteArray &numericFileId) const;

    /** Holds the accounts credentials */
    [[nodiscard]] AbstractCredentials *credentials() const;
    void setCredentials(AbstractCredentials *cred);

    /** Create a network request on the account's QNAM.
     *
     * Network requests in AbstractNetworkJobs are created through
     * this function. Other places should prefer to use jobs or
     * sendRequest().
     */
    QNetworkReply *sendRawRequest(const QByteArray &verb,
        const QUrl &url,
        QNetworkRequest req = QNetworkRequest(),
        QIODevice *data = nullptr);

    QNetworkReply *sendRawRequest(const QByteArray &verb,
        const QUrl &url, QNetworkRequest req, const QByteArray &data);

    QNetworkReply *sendRawRequest(const QByteArray &verb,
        const QUrl &url, QNetworkRequest req, QHttpMultiPart *data);

    /** Create and start network job for a simple one-off request.
     *
     * More complicated requests typically create their own job types.
     */
    SimpleNetworkJob *sendRequest(const QByteArray &verb,
        const QUrl &url,
        QNetworkRequest req = QNetworkRequest(),
        QIODevice *data = nullptr);

    /** The ssl configuration during the first connection */
    QSslConfiguration getOrCreateSslConfig();
    [[nodiscard]] QSslConfiguration sslConfiguration() const { return _sslConfiguration; }
    void setSslConfiguration(const QSslConfiguration &config);
    // Because of bugs in Qt, we use this to store info needed for the SSL Button
    QSslCipher _sessionCipher;
    QByteArray _sessionTicket;
    QList<QSslCertificate> _peerCertificateChain;


    /** The certificates of the account */
    [[nodiscard]] QList<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> certs);
    void addApprovedCerts(const QList<QSslCertificate> certs);

    // Usually when a user explicitly rejects a certificate we don't
    // ask again. After this call, a dialog will again be shown when
    // the next unknown certificate is encountered.
    void resetRejectedCertificates();

    // pluggable handler
    void setSslErrorHandler(AbstractSslErrorHandler *handler);

    // To be called by credentials only, for storing username and the like
    [[nodiscard]] QVariant credentialSetting(const QString &key) const;
    void setCredentialSetting(const QString &key, const QVariant &value);

    /** Assign a client certificate */
    void setCertificate(const QByteArray certificate = QByteArray(), const QString privateKey = QString());

    /** Access the server capabilities */
    [[nodiscard]] const Capabilities &capabilities() const;
    void setCapabilities(const QVariantMap &caps);

    /** Access the server version
     *
     * For servers >= 10.0.0, this can be the empty string until capabilities
     * have been received.
     */
    [[nodiscard]] QString serverVersion() const;

    // check if the checksum validation of E2EE metadata is allowed to be skipped via config file, this will only work before client 3.9.0
    [[nodiscard]] bool shouldSkipE2eeMetadataChecksumValidation() const;
    void resetShouldSkipE2eeMetadataChecksumValidation();

    /** Server version for easy comparison.
     *
     * Example: serverVersionInt() >= makeServerVersion(11, 2, 3)
     *
     * Will be 0 if the version is not available yet.
     */
    [[nodiscard]] int serverVersionInt() const;

    static constexpr int makeServerVersion(const int majorVersion, const int minorVersion, const int patchVersion) {
        return (majorVersion << 16) + (minorVersion << 8) + patchVersion;
    };

    void setServerVersion(const QString &version);

    /** Whether the server is too old.
     *
     * Not supporting server versions is a gradual process. There's a hard
     * compatibility limit (see ConnectionValidator) that forbids connecting
     * to extremely old servers. And there's a weak "untested, not
     * recommended, potentially dangerous" limit, that users might want
     * to go beyond.
     *
     * This function returns true if the server is beyond the weak limit.
     */
    [[nodiscard]] bool serverVersionUnsupported() const;

    [[nodiscard]] bool secureFileDropSupported() const;

    [[nodiscard]] bool isUsernamePrefillSupported() const;

    [[nodiscard]] bool isChecksumRecalculateRequestSupported() const;

    [[nodiscard]] int checksumRecalculateServerVersionMinSupportedMajor() const;

    /** True when the server connection is using HTTP2  */
    bool isHttp2Supported() { return _http2Supported; }
    void setHttp2Supported(bool value) { _http2Supported = value; }

    void clearCookieJar();
    void lendCookieJarTo(QNetworkAccessManager *guest);
    QString cookieJarPath();

    void resetNetworkAccessManager();
    QNetworkAccessManager *networkAccessManager();
    QSharedPointer<QNetworkAccessManager> sharedNetworkAccessManager();

    /// Called by network jobs on credential errors, emits invalidCredentials()
    void handleInvalidCredentials();

    ClientSideEncryption* e2e();

    /// Used in RemoteWipe
    void retrieveAppPassword();
    void writeAppPasswordOnce(QString appPassword);
    void deleteAppPassword();

    void deleteAppToken();

    /// Direct Editing
    // Check for the directEditing capability
    void fetchDirectEditors(const QUrl &directEditingURL, const QString &directEditingETag);

    void setupUserStatusConnector();
    void trySetupPushNotifications();
    [[nodiscard]] PushNotifications *pushNotifications() const;
    void setPushNotificationsReconnectInterval(int interval);

    void trySetupClientStatusReporting();

    void reportClientStatus(const ClientStatusReportingStatus status) const;

    [[nodiscard]] std::shared_ptr<UserStatusConnector> userStatusConnector() const;

    void setLockFileState(const QString &serverRelativePath,
                          const QString &remoteSyncPathWithTrailingSlash,
                          const QString &localSyncPath,
                          SyncJournalDb * const journal,
                          const SyncFileItem::LockStatus lockStatus,
                          const SyncFileItem::LockOwnerType lockOwnerType);

    SyncFileItem::LockStatus fileLockStatus(SyncJournalDb * const journal,
                                            const QString &folderRelativePath) const;

    bool fileCanBeUnlocked(SyncJournalDb * const journal, const QString &folderRelativePath) const;

    void setTrustCertificates(bool trustCertificates);
    [[nodiscard]] bool trustCertificates() const;

    void setE2eEncryptionKeysGenerationAllowed(bool allowed);
    [[nodiscard]] bool e2eEncryptionKeysGenerationAllowed() const;

    [[nodiscard]] bool askUserForMnemonic() const;

public slots:
    /// Used when forgetting credentials
    void clearQNAMCache();
    void slotHandleSslErrors(QNetworkReply *, QList<QSslError>);
    void setAskUserForMnemonic(const bool ask);

signals:
    /// Emitted whenever there's network activity
    void propagatorNetworkActivity();

    /// Triggered by handleInvalidCredentials()
    void invalidCredentials();

    void credentialsFetched(OCC::AbstractCredentials *credentials);
    void credentialsAsked(OCC::AbstractCredentials *credentials);

    /// Forwards from QNetworkAccessManager::proxyAuthenticationRequired().
    void proxyAuthenticationRequired(const QNetworkProxy &, QAuthenticator *);

    // e.g. when the approved SSL certificates changed
    void wantsAccountSaved(OCC::Account *acc);

    void serverVersionChanged(OCC::Account *account, const QString &newVersion, const QString &oldVersion);

    void accountChangedAvatar();
    void accountChangedDisplayName();
    void prettyNameChanged();
    void askUserForMnemonicChanged();

    /// Used in RemoteWipe
    void appPasswordRetrieved(QString);

    void pushNotificationsReady(OCC::Account *account);
    void pushNotificationsDisabled(OCC::Account *account);

    void userStatusChanged();

    void serverUserStatusChanged();

    void capabilitiesChanged();

    void lockFileSuccess();
    void lockFileError(const QString&);

protected Q_SLOTS:
    void slotCredentialsFetched();
    void slotCredentialsAsked();
    void slotDirectEditingRecieved(const QJsonDocument &json);

private slots:
    void removeLockStatusChangeInprogress(const QString &serverRelativePath, const SyncFileItem::LockStatus lockStatus);

private:
    Account(QObject *parent = nullptr);
    void setSharedThis(AccountPtr sharedThis);
    void updateServerColors();

    [[nodiscard]] static QString davPathBase();
    [[nodiscard]] QColor serverColor() const;

    bool _trustCertificates = false;

    bool _e2eEncryptionKeysGenerationAllowed = false;
    bool _e2eAskUserForMnemonic = false;

    QWeakPointer<Account> _sharedThis;
    QString _id;
    QString _davUser;
    QString _displayName;
    QTimer _pushNotificationsReconnectTimer;
#ifndef TOKEN_AUTH_ONLY
    QImage _avatarImg;
#endif
    QMap<QString, QVariant> _settingsMap;
    QUrl _url;

    /** If url to use for any user-visible urls.
     *
     * If the server configures overwritehost this can be different from
     * the connection url in _url. We retrieve the visible host through
     * the ocs/v1.php/config endpoint in ConnectionValidator.
     */
    QUrl _userVisibleUrl;

    QList<QSslCertificate> _approvedCerts;
    QSslConfiguration _sslConfiguration;
    Capabilities _capabilities;
    QString _serverVersion;
    QColor _serverColor;
    QColor _serverTextColor = QColorConstants::White;
    bool _skipE2eeMetadataChecksumValidation = false;
    QScopedPointer<AbstractSslErrorHandler> _sslErrorHandler;
    QSharedPointer<QNetworkAccessManager> _am;
    QScopedPointer<AbstractCredentials> _credentials;
    bool _http2Supported = false;

    /// Certificates that were explicitly rejected by the user
    QList<QSslCertificate> _rejectedCertificates;

    static QString _configFileName;

    ClientSideEncryption _e2e;

    /// Used in RemoteWipe
    bool _wroteAppPassword = false;

    friend class AccountManager;

    // Direct Editing
    QString _lastDirectEditingETag;

    PushNotifications *_pushNotifications = nullptr;

    std::unique_ptr<ClientStatusReporting> _clientStatusReporting;

    std::shared_ptr<UserStatusConnector> _userStatusConnector;

    QHash<QString, QVector<SyncFileItem::LockStatus>> _lockStatusChangeInprogress;

    /* IMPORTANT - remove later - FIXME MS@2019-12-07 -->
     * TODO: For "Log out" & "Remove account": Remove client CA certs and KEY!
     *
     *       Disabled as long as selecting another cert is not supported by the UI.
     *
     *       Being able to specify a new certificate is important anyway: expiry etc.
     *
     *       We introduce this dirty hack here, to allow deleting them upon Remote Wipe.
    */
    public:
        void setRemoteWipeRequested_HACK() { _isRemoteWipeRequested_HACK = true; }
        bool isRemoteWipeRequested_HACK() { return _isRemoteWipeRequested_HACK; }
    private:
        bool _isRemoteWipeRequested_HACK = false;
    // <-- FIXME MS@2019-12-07
};
}

Q_DECLARE_METATYPE(OCC::AccountPtr)
Q_DECLARE_METATYPE(OCC::Account *)

#endif //SERVERCONNECTION_H
