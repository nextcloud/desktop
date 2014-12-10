/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H
#define MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "creds/abstractcredentials.h"

namespace QKeychain {
    class Job;
}

class QAuthenticator;

namespace OCC
{

class ShibbolethWebView;

class OWNCLOUDSYNC_EXPORT ShibbolethCredentials : public AbstractCredentials
{
Q_OBJECT

public:
    ShibbolethCredentials();

    /* create a credentials for an already connected account */
    ShibbolethCredentials(const QNetworkCookie &cookie, Account *acc);

    void syncContextPreInit(CSYNC* ctx) Q_DECL_OVERRIDE;
    void syncContextPreStart(CSYNC* ctx) Q_DECL_OVERRIDE;
    bool changed(AbstractCredentials* credentials) const Q_DECL_OVERRIDE;
    QString authType() const Q_DECL_OVERRIDE;
    QString user() const Q_DECL_OVERRIDE;
    QNetworkAccessManager* getQNAM() const Q_DECL_OVERRIDE;
    bool ready() const Q_DECL_OVERRIDE;
    void fetch(Account *account) Q_DECL_OVERRIDE;
    bool stillValid(QNetworkReply *reply) Q_DECL_OVERRIDE;
    void persist(Account *account) Q_DECL_OVERRIDE;
    void invalidateToken(Account *account) Q_DECL_OVERRIDE;

    void showLoginWindow(Account*);

    static QList<QNetworkCookie> accountCookies(Account*);
    static QNetworkCookie findShibCookie(Account*, QList<QNetworkCookie> cookies = QList<QNetworkCookie>());
    static QByteArray shibCookieName();

public Q_SLOTS:
    void invalidateAndFetch(Account *account) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onShibbolethCookieReceived(const QNetworkCookie&, Account*);
    void slotBrowserRejected();
    void onFetched();
    void slotReadJobDone(QKeychain::Job*);
    void slotInvalidateAndFetchInvalidateDone(QKeychain::Job*);
    void slotReplyFinished(QNetworkReply*);
    void slotUserFetched(const QString& user);
    void slotFetchUser();

Q_SIGNALS:
    void newCookie(const QNetworkCookie& cookie);
    void invalidatedAndFetched(const QByteArray& cookieData);

private:
    void storeShibCookie(const QNetworkCookie &cookie, Account *account);
    void removeShibCookie(Account *account);
    void addToCookieJar(const QNetworkCookie &cookie);
    QUrl _url;
    QByteArray prepareCookieData() const;

    bool _ready;
    bool _stillValid;
    bool _fetchJobInProgress;
    QPointer<ShibbolethWebView> _browser;
    QNetworkCookie _shibCookie;
    QString _user;
};

} // namespace OCC

#endif
