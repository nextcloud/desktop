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

class OWNCLOUDSYNC_EXPORT AccountManager : public QObject {
    Q_OBJECT
public:
    static AccountManager *instance();
    ~AccountManager() {}

    void setAccount(AccountPtr account);
    AccountPtr account() { return _account; }

Q_SIGNALS:
    void accountAdded(AccountPtr account);
    void accountRemoved(AccountPtr account);

private:
    AccountManager() {}
    AccountPtr _account;
    static AccountManager *_instance;
};

/* Reimplement this to handle SSL errors */
class AbstractSslErrorHandler {
public:
    virtual ~AbstractSslErrorHandler() {}
    virtual bool handleErrors(QList<QSslError>, QList<QSslCertificate>*, AccountPtr) = 0;
};

/**
 * @brief This class represents an account on an ownCloud Server
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

    /**
     * Saves the account to a given settings file
     */
    void save();

    /**
     * Creates an account object from from a given settings file.
     */
    static AccountPtr restore();

    /**
     * @brief Checks the Account instance is different from \param other
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
    QSslConfiguration createSslConfig();
    QSslConfiguration sslConfiguration() const { return _sslConfiguration; }
    void setSslConfiguration(const QSslConfiguration &config);
    /** The certificates of the account */
    QList<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> certs);
    void addApprovedCerts(const QList<QSslCertificate> certs);

    // pluggable handler
    void setSslErrorHandler(AbstractSslErrorHandler *handler);

    // static helper function
    static QUrl concatUrlPath(const QUrl &url, const QString &concatPath,
                              const QList< QPair<QString, QString> > &queryItems = (QList<QPair<QString, QString>>()));

    /**  Returns a new settings pre-set in a specific group.  The Settings will be created
         with the given parent. If no parents is specified, the caller must destroy the settings */
    static QSettings* settingsWithGroup(const QString &group, QObject *parent = 0);

    // to be called by credentials only
    QVariant credentialSetting(const QString& key) const;
    void setCredentialSetting(const QString& key, const QVariant &value);

    void setCertificate(const QByteArray certficate = QByteArray(), const QString privateKey = QString());

    void setCapabilities(const QVariantMap &caps);
    QVariantMap capabilities();
    void setServerVersion(const QString &version);
    QString serverVersion();

    void clearCookieJar();

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
};

}

Q_DECLARE_METATYPE(OCC::AccountPtr)

#endif //SERVERCONNECTION_H
