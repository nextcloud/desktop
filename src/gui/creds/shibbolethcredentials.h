/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H
#define MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H

#include <QList>
#include <QLoggingCategory>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "creds/abstractcredentials.h"

namespace QKeychain {
class Job;
}

class QAuthenticator;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcShibboleth)

class ShibbolethWebView;

/**
 * @brief The ShibbolethCredentials class
 * @ingroup gui
 */
class ShibbolethCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    ShibbolethCredentials();

    /* create credentials for an already connected account */
    ShibbolethCredentials(const QNetworkCookie &cookie);

    void setAccount(Account *account) Q_DECL_OVERRIDE;
    QString authType() const Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QNetworkAccessManager *createQNAM() const Q_DECL_OVERRIDE;
    bool ready() const Q_DECL_OVERRIDE;
    void fetchFromKeychain() Q_DECL_OVERRIDE;
    void askFromUser() Q_DECL_OVERRIDE;
    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void persist() Q_DECL_OVERRIDE;
    void invalidateToken() Q_DECL_OVERRIDE;
    void forgetSensitiveData() Q_DECL_OVERRIDE;

    void showLoginWindow();

    static QList<QNetworkCookie> accountCookies(Account *);
    static QNetworkCookie findShibCookie(Account *, QList<QNetworkCookie> cookies = QList<QNetworkCookie>());
    static QByteArray shibCookieName();

private Q_SLOTS:
    void onShibbolethCookieReceived(const QNetworkCookie &);
    void slotBrowserRejected();
    void slotReadJobDone(QKeychain::Job *);
    void slotReplyFinished(QNetworkReply *);
    void slotUserFetched(const QString &user);
    void slotFetchUser();
    void slotFetchUserHelper();

Q_SIGNALS:
    void newCookie(const QNetworkCookie &cookie);

private:
    void storeShibCookie(const QNetworkCookie &cookie);
    void removeShibCookie();
    void addToCookieJar(const QNetworkCookie &cookie);

    /// Reads data from keychain, progressing to slotReadJobDone
    void fetchFromKeychainHelper();

    QUrl _url;
    QByteArray prepareCookieData() const;

    bool _ready;
    bool _stillValid;
    QPointer<ShibbolethWebView> _browser;
    QNetworkCookie _shibCookie;
    QString _user;
    bool _keychainMigration;
};

} // namespace OCC

#endif
