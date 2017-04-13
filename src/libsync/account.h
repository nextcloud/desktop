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
#include <QPixmap>

#include "utility.h"
#include <memory>
#include "capabilities.h"

class QSettings;
class QNetworkReply;
class QUrl;
class QNetworkAccessManager;

namespace OCC {

class AbstractCredentials;
class Account;
typedef QSharedPointer<Account> AccountPtr;
class QuotaInfo;
class AccessManager;


/**
 * @brief Reimplement this to handle SSL errors from libsync
 * @ingroup libsync
 */
class AbstractSslErrorHandler {
public:
    virtual ~AbstractSslErrorHandler() {}
    virtual bool handleErrors(QList<QSslError>, const QSslConfiguration &conf, QList<QSslCertificate>*, AccountPtr) = 0;
};

/**
 * @brief The Account class represents an account on an ownCloud Server
 * @ingroup libsync
 *
 * The Account has a name and url. It also has information about credentials,
 * SSL errors and certificates.
 */
class OWNCLOUDSYNC_EXPORT Account : public QObject {
    Q_OBJECT
public:
    static AccountPtr create();
    ~Account();

    AccountPtr sharedFromThis();

    /**
     * The user that can be used in dav url.
     *
     * This can very well be different frome the login user that's
     * stored in credentials()->user().
     */
    QString davUser() const;
    void setDavUser(const QString &newDavUser);

    QImage avatar() const;
    void setAvatar(const QImage& img);

    /// The name of the account as shown in the toolbar
    QString displayName() const;

    /// The internal id of the account.
    QString id() const;

    /** Server url of the account */
    void setUrl(const QUrl &url);
    QUrl url() const { return _url; }

    /**
     * @brief The possibly themed dav path for the account. It has
     *        a trailing slash.
     * @returns the (themeable) dav path for the account.
     */
    QString davPath() const;
    void setDavPath(const QString&s) { _davPath = s; }
    void setNonShib(bool nonShib);

    /** Returns webdav entry URL, based on url() */
    QUrl davUrl() const;

    /** Holds the accounts credentials */
    AbstractCredentials* credentials() const;
    void setCredentials(AbstractCredentials *cred);


    // For creating various network requests
    QNetworkReply* sendRequest(const QByteArray &verb,
                               const QUrl &url,
                               QNetworkRequest req = QNetworkRequest(),
                               QIODevice *data = 0);

    /** The ssl configuration during the first connection */
    QSslConfiguration getOrCreateSslConfig();
    QSslConfiguration sslConfiguration() const { return _sslConfiguration; }
    void setSslConfiguration(const QSslConfiguration &config);
    // Because of bugs in Qt, we use this to store info needed for the SSL Button
    QSslCipher _sessionCipher;
    QByteArray _sessionTicket;
    QList<QSslCertificate> _peerCertificateChain;


    /** The certificates of the account */
    QList<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> certs);
    void addApprovedCerts(const QList<QSslCertificate> certs);

    // Usually when a user explicitly rejects a certificate we don't
    // ask again. After this call, a dialog will again be shown when
    // the next unknown certificate is encountered.
    void resetRejectedCertificates();

    // pluggable handler
    void setSslErrorHandler(AbstractSslErrorHandler *handler);

    // To be called by credentials only, for storing username and the like
    QVariant credentialSetting(const QString& key) const;
    void setCredentialSetting(const QString& key, const QVariant &value);

    /** Assign a client certificate */
    void setCertificate(const QByteArray certficate = QByteArray(), const QString privateKey = QString());

    /** Access the server capabilities */
    const Capabilities &capabilities() const;
    void setCapabilities(const QVariantMap &caps);

    /** Access the server version */
    QString serverVersion() const;
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

    // Fixed from 8.1 https://github.com/owncloud/client/issues/3730
    /** Detects a specific bug in older server versions */
    bool rootEtagChangesNotOnlySubFolderEtags();

    void clearCookieJar();
    void lendCookieJarTo(QNetworkAccessManager *guest);
    QString cookieJarPath();

    void resetNetworkAccessManager();
    QNetworkAccessManager* networkAccessManager();

    /// Called by network jobs on credential errors, emits invalidCredentials()
    void handleInvalidCredentials();

signals:
    /// Emitted whenever there's network activity
    void propagatorNetworkActivity();

    /// Triggered by handleInvalidCredentials()
    void invalidCredentials();

    void credentialsFetched(AbstractCredentials* credentials);
    void credentialsAsked(AbstractCredentials* credentials);

    /// Forwards from QNetworkAccessManager::proxyAuthenticationRequired().
    void proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*);

    // e.g. when the approved SSL certificates changed
    void wantsAccountSaved(Account* acc);

    void serverVersionChanged(Account* account, const QString& newVersion, const QString& oldVersion);

    void accountChangedAvatar();

protected Q_SLOTS:
    void slotHandleSslErrors(QNetworkReply*,QList<QSslError>);
    void slotCredentialsFetched();
    void slotCredentialsAsked();

private:
    Account(QObject *parent = 0);
    void setSharedThis(AccountPtr sharedThis);

    QWeakPointer<Account> _sharedThis;
    QString _id;
    QString _davUser;
    QImage _avatarImg;
    QMap<QString, QVariant> _settingsMap;
    QUrl _url;
    QList<QSslCertificate> _approvedCerts;
    QSslConfiguration _sslConfiguration;
    Capabilities _capabilities;
    QString _serverVersion;
    QScopedPointer<AbstractSslErrorHandler> _sslErrorHandler;
    QuotaInfo *_quotaInfo;
    QSharedPointer<QNetworkAccessManager> _am;
    QSharedPointer<AbstractCredentials> _credentials;

    /// Certificates that were explicitly rejected by the user
    QList<QSslCertificate> _rejectedCertificates;

    static QString _configFileName;

    QString _davPath; // defaults to value from theme, might be overwritten in brandings
    friend class AccountManager;
};

}

Q_DECLARE_METATYPE(OCC::AccountPtr)

#endif //SERVERCONNECTION_H
