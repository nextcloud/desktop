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
#include <QSslCertificate>
#include <QSslError>

class QSettings;
class QNetworkReply;
class QUrl;
class QNetworkAccessManager;

namespace Mirall {

class AbstractCredentials;
class Account;

class AccountManager {
public:
    static AccountManager *instance();
    ~AccountManager();

    void setAccount(Account *account) { _account = account; }
    Account *account() { return _account; }

private:
    AccountManager() {}
    Account *_account;
    static AccountManager *_instance;
};

/* Reimplement this to handle SSL errors */
class AbstractSslErrorHandler {
public:
    virtual bool handleErrors(QList<QSslError>, QList<QSslCertificate>*, Account*) = 0;
};

/**
 * @brief This class represents an account on an ownCloud Server
 */
class Account : public QObject {
    Q_OBJECT
public:
    Account(QObject *parent = 0);
    /**
     * Saves the account to a given settings file
     */
    void save(QSettings &settings);

    /**
     * Creates an account object from from a given settings file.
     */
    static Account* restore(QSettings &settings);

    /** Holds the accounts credentials */
    AbstractCredentials* credentials() const;
    void setCredentials(AbstractCredentials *cred);

    /** Server url of the account */
    void setUrl(const QUrl &url);
    QUrl url() const { return _url; }

    /** User name of the account */
    void setUser(const QString &user);
    QString user() const { return _user; }

    /** Returns webdav entry URL, based on url() */
    QUrl davUrl() const;

    QList<QNetworkCookie> lastAuthCookies() const;

    QNetworkReply* getRequest(const QString &relPath);
    QNetworkReply* davRequest(const QByteArray &verb, const QString &relPath, QNetworkRequest req, QIODevice *data = 0);

    /** The certificates of the account */
    QList<QSslCertificate> certificateChain() const { return _certificateChain; }
    void setCertificateChain(const QList<QSslCertificate> &certs);
    /** The certificates of the account */
    QList<QSslCertificate> approvedCerts() const { return _approvedCerts; }
    void setApprovedCerts(const QList<QSslCertificate> certs);

    AbstractSslErrorHandler* sslErrorHandler() const { return _sslErrorHandler; }
    void setSslErrorHandler(AbstractSslErrorHandler *handler);

    static QUrl concatUrlPath(const QUrl &url, const QString &concatPath);

private slots:
    void slotHandleErrors(QNetworkReply*,QList<QSslError>);

private:
    QNetworkAccessManager *_am;
    QList<QSslCertificate> _caCerts;
    QUrl _url;
    QString _user;
    AbstractCredentials* _credentials;
    QList<QSslCertificate> _approvedCerts;
    QList<QSslCertificate> _certificateChain;
    bool _treatSslErrorsAsFailure;
    AbstractSslErrorHandler *_sslErrorHandler;
};

}

#endif //SERVERCONNECTION_H
