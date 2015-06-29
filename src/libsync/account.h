/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <QSslError>
#include <QSharedPointer>
#include "utility.h"
#include <memory>

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
 */
class OWNCLOUDSYNC_EXPORT Account : public QObject {
    Q_OBJECT
public:
    QString davPath() const { return _davPath; }
    void setDavPath(const QString&s) { _davPath = s; }

    static AccountPtr create();
    ~Account();

    void setSharedThis(AccountPtr sharedThis);
    AccountPtr sharedFromThis();

    /// The name of the account as shown in the toolbar
    QString displayName() const;

    /// The internal id of the account.
    QString id() const;

    /**
     * @brief Checks the Account instance is different from @param other
     *
     * @returns true, if credentials or url have changed, false otherwise
     */
    bool changed(AccountPtr other, bool ignoreUrlProtocol) const;

    /** Holds the accounts credentials */
    AbstractCredentials* credentials() const;
    void setCredentials(AbstractCredentials *cred);

    /** Server url of the account */
    void setUrl(const QUrl &url);
    QUrl url() const { return _url; }

    /** Returns webdav entry URL, based on url() */
    QUrl davUrl() const;

    /** set and retrieve the migration flag: if an account of a branded
     *  client was migrated from a former ownCloud Account, this is true
     */
    void setMigrated(bool mig);
    bool wasMigrated();

    QList<QNetworkCookie> lastAuthCookies() const;

    QNetworkReply* headRequest(const QString &relPath);
    QNetworkReply* headRequest(const QUrl &url);
    QNetworkReply* getRequest(const QString &relPath);
    QNetworkReply* getRequest(const QUrl &url);
    QNetworkReply* davRequest(const QByteArray &verb, const QString &relPath, QNetworkRequest req, QIODevice *data = 0);
    QNetworkReply* davRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data = 0);

    /** The ssl configuration during the first connection */
    QSslConfiguration getOrCreateSslConfig();
    QSslConfiguration sslConfiguration() const { return _sslConfiguration; }
    void setSslConfiguration(const QSslConfiguration &config);
    /** The certificates of the account */
    QList<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> certs);
    void addApprovedCerts(const QList<QSslCertificate> certs);

    // Usually when a user explicitly rejects a certificate we don't
    // ask again. After this call, a dialog will again be shown when
    // the next unknown certificate is encountered.
    void resetSslCertErrorState();

    // pluggable handler
    void setSslErrorHandler(AbstractSslErrorHandler *handler);

    // static helper function
    static QUrl concatUrlPath(const QUrl &url, const QString &concatPath,
                              const QList< QPair<QString, QString> > &queryItems = (QList<QPair<QString, QString>>()));

    /**  Returns a new settings pre-set in a specific group.  The Settings will be created
         with the given parent. If no parents is specified, the caller must destroy the settings */
    static std::unique_ptr<QSettings> settingsWithGroup(const QString& group, QObject* parent = 0);

    // to be called by credentials only
    QVariant credentialSetting(const QString& key) const;
    void setCredentialSetting(const QString& key, const QVariant &value);

    void setCertificate(const QByteArray certficate = QByteArray(), const QString privateKey = QString());

    void setCapabilities(const QVariantMap &caps);
    QVariantMap capabilities();
    void setServerVersion(const QString &version);
    QString serverVersion();

    void clearCookieJar();
    void lendCookieJarTo(QNetworkAccessManager *guest);

    void resetNetworkAccessManager();
    QNetworkAccessManager* networkAccessManager();

    /// Called by network jobs on credential errors.
    void handleInvalidCredentials();

signals:
    void propagatorNetworkActivity();
    void invalidCredentials();
    void credentialsFetched(AbstractCredentials* credentials);

protected Q_SLOTS:
    void slotHandleErrors(QNetworkReply*,QList<QSslError>);
    void slotCredentialsFetched();

private:
    Account(QObject *parent = 0);

    QWeakPointer<Account> _sharedThis;
    QString _id;
    QMap<QString, QVariant> _settingsMap;
    QUrl _url;
    QList<QSslCertificate> _approvedCerts;
    QSslConfiguration _sslConfiguration;
    QVariantMap _capabilities;
    QString _serverVersion;
    QScopedPointer<AbstractSslErrorHandler> _sslErrorHandler;
    QuotaInfo *_quotaInfo;
    QNetworkAccessManager *_am;
    AbstractCredentials* _credentials;
    bool _treatSslErrorsAsFailure;
    static QString _configFileName;
    QByteArray _pemCertificate; 
    QString _pemPrivateKey;  
    QString _davPath; // default "remote.php/webdav/";
    bool _wasMigrated;
    friend class AccountManager;
};

}

Q_DECLARE_METATYPE(OCC::AccountPtr)

#endif //SERVERCONNECTION_H
