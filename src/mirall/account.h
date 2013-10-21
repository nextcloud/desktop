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

class QSettings;
class QNetworkReply;
class QUrl;

namespace Mirall {

class MirallAccessManager;
class AbstractCredentials;
class Account;

class AccountManager {
public:
    static AccountManager *instance();
    ~AccountManager();

    void setAccount(Account *account) { _account = account; }
    Account *account() { return _account; }

private:
    AccountManager();
    Account *_account;
    static AccountManager *_instance;
};

/**
 * @brief This class represents an account on an ownCloud Server
 */
class Account : public QObject {
public:
    /**
     * Saves the account to a given settings file
     */
    void save(QSettings &settings);

    /**
     * Creates an account object from from a given settings file.
     */
    static Account* restore(QSettings settings);

    /** Holds the accounts credentials */
    AbstractCredentials* credentials() const;
    void setCredentials(AbstractCredentials *cred);

    /** Server url of the account */
    void setUrl(const QUrl &url);
    QUrl url() const;

    /** Returns webdav entry URL, based on url() */
    QUrl davUrl() const;

    QList<QNetworkCookie> lastAuthCookies() const;

    QNetworkReply* getRequest(const QString &relPath);
    QNetworkReply* davRequest(const QString &relPath, const QByteArray &verb, QIODevice *data = 0);

    /** The certificates of the account */
    QByteArray caCerts() const;
    void setCaCerts(const QByteArray &certs);

protected:
    QUrl concatUrlPath(const QUrl &url, const QString &concatPath) const;

private:
    Account(QObject *parent = 0);
    MirallAccessManager *_am;
    QByteArray _caCerts;
    QUrl _url;
    AbstractCredentials* _credentials;
};

}

#endif //SERVERCONNECTION_H
