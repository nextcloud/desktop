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
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkRequest>
#include <QSharedPointer>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QUrl>
#include <QUuid>

#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#endif

#include "common/utility.h"
#include <memory>
#include "capabilities.h"
#include "jobqueue.h"

class QSettings;
class QNetworkReply;
class QUrl;
class AccessManager;

namespace OCC {

class CredentialManager;
class AbstractCredentials;
class Account;
typedef QSharedPointer<Account> AccountPtr;
class QuotaInfo;
class AccessManager;
class SimpleNetworkJob;


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
    Q_PROPERTY(QUrl url MEMBER _url)

public:
    static AccountPtr create();
    ~Account() override;

    AccountPtr sharedFromThis();

    /**
     * The user that can be used in dav url.
     *
     * This can very well be different frome the login user that's
     * stored in credentials()->user().
     */
    QString davUser() const;
    void setDavUser(const QString &newDavUser);

    /***
     * With OC 10 this is the equivalent to the sync root.
     * With ocis and spaces this will be the default folder containing all spaces.
     * This function will assert if the sync root is empty.
     */
    QString defaultSyncRoot() const;

    /***
     * Whether we have defaultSyncRoot defined.
     */
    bool hasDefaultSyncRoot() const;

    /***
     * Set defaultSyncRoot and creates the path on the filesystem.
     * Setting an empty string will have no effect.
     */
    void setDefaultSyncRoot(const QString &syncRoot);

    QString davDisplayName() const;
    void setDavDisplayName(const QString &newDisplayName);

#ifndef TOKEN_AUTH_ONLY
    QPixmap avatar() const;
    void setAvatar(const QPixmap &img);
#endif

    /// The name of the account as shown in the toolbar
    QString displayName() const;

    /// The internal id of the account.
    Q_DECL_DEPRECATED_X("Use uuid") QString id() const;

    /** Server url of the account */
    void setUrl(const QUrl &url);
    QUrl url() const { return _url; }

    /**
     * @brief The possibly themed dav path for the account. It has
     *        a trailing slash.
     * @returns the (themeable) dav path for the account.
     */
    QString davPath() const;

    /** Returns webdav entry URL, based on url() */
    QUrl davUrl() const;

    /** Holds the accounts credentials */
    AbstractCredentials *credentials() const;
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

    // Because of bugs in Qt, we use this to store info needed for the SSL Button
    QSslCipher _sessionCipher;
    QByteArray _sessionTicket;
    QSet<QSslCertificate> _peerCertificateChain;


    /** The certificates of the account */
    QSet<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> &certs);
    void addApprovedCerts(const QSet<QSslCertificate> &certs);

    // To be called by credentials only, for storing username and the like
    QVariant credentialSetting(const QString &key) const;
    void setCredentialSetting(const QString &key, const QVariant &value);

    /** Access the server capabilities */
    const Capabilities &capabilities() const;
    void setCapabilities(const Capabilities &caps);

    bool hasCapabilities() const;

    /** Access the server version
     *
     * For servers >= 10.0.0, this can be the empty string until capabilities
     * have been received.
     */
    QString serverVersion() const;

    /** Server version for easy comparison.
     *
     * Example: serverVersionInt() >= makeServerVersion(11, 2, 3)
     *
     * Will be 0 if the version is not available yet.
     */
    int serverVersionInt() const;

    static int makeServerVersion(int majorVersion, int minorVersion, int patchVersion);
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
    bool serverVersionUnsupported() const;

    /** True when the server connection is using HTTP2  */
    bool isHttp2Supported() { return _http2Supported; }
    void setHttp2Supported(bool value) { _http2Supported = value; }

    void clearCookieJar();
    void lendCookieJarTo(QNetworkAccessManager *guest);
    QString cookieJarPath();

    void resetAccessManager();
    AccessManager *accessManager();
    [[deprecated]] QSharedPointer<AccessManager> sharedAccessManager();

    JobQueue *jobQueue();

    QUuid uuid() const;

    CredentialManager *credentialManager() const;

public slots:
    /// Used when forgetting credentials
    void clearAMCache();

signals:
    /// Triggered by handleInvalidCredentials()
    void invalidCredentials();

    void credentialsFetched(AbstractCredentials *credentials);
    void credentialsAsked(AbstractCredentials *credentials);

    /// Forwards from QNetworkAccessManager::proxyAuthenticationRequired().
    void proxyAuthenticationRequired(const QNetworkProxy &, QAuthenticator *);

    // e.g. when the approved SSL certificates changed
    void wantsAccountSaved(Account *acc);

    void serverVersionChanged(Account *account, const QString &newVersion, const QString &oldVersion);

    void accountChangedAvatar();
    void accountChangedDisplayName();

    void unknownConnectionState();

    void requestUrlUpdate(const QUrl &newUrl);

protected Q_SLOTS:
    void slotCredentialsFetched();
    void slotCredentialsAsked();

private:
    Account(QObject *parent = nullptr);
    void setSharedThis(AccountPtr sharedThis);

    QWeakPointer<Account> _sharedThis;
    QString _id;
    QUuid _uuid;
    QString _davUser;
    QString _displayName;
    QString _defaultSyncRoot;
#ifndef TOKEN_AUTH_ONLY
    QPixmap _avatarImg;
#endif
    QMap<QString, QVariant> _settingsMap;
    QUrl _url;

    QSet<QSslCertificate> _approvedCerts;
    Capabilities _capabilities;
    QString _serverVersion;
    QuotaInfo *_quotaInfo;
    QSharedPointer<AccessManager> _am;
    QScopedPointer<AbstractCredentials> _credentials;
    bool _http2Supported = false;

    JobQueue _jobQueue;
    JobQueueGuard _queueGuard;
    CredentialManager *_credentialManager;
    friend class AccountManager;
};
}

Q_DECLARE_METATYPE(OCC::AccountPtr)

#endif //SERVERCONNECTION_H
