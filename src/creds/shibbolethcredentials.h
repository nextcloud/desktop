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

#include "creds/abstractcredentials.h"

namespace QKeychain {
    class Job;
}

namespace Mirall
{

class ShibbolethWebView;

class ShibbolethCredentials : public AbstractCredentials
{
Q_OBJECT

public:
    ShibbolethCredentials();
    ShibbolethCredentials(const QNetworkCookie& cookie, const QMap<QUrl, QList<QNetworkCookie> >& otherCookies);

    void syncContextPreInit(CSYNC* ctx);
    void syncContextPreStart(CSYNC* ctx);
    bool changed(AbstractCredentials* credentials) const;
    QString authType() const;
    QString user() const;
    QNetworkAccessManager* getQNAM() const;
    bool ready() const;
    void fetch(Account *account);
    bool stillValid(QNetworkReply *reply);
    void persist(Account *account);
    void invalidateToken(Account *account);

    QNetworkCookie cookie() const;

public Q_SLOTS:
    void invalidateAndFetch(Account *account);

private Q_SLOTS:
    void onShibbolethCookieReceived(const QNetworkCookie& cookie, Account*);
    void slotBrowserHidden();
    void onFetched();
    void slotReadJobDone(QKeychain::Job*);
    void slotInvalidateAndFetchInvalidateDone(QKeychain::Job*);

Q_SIGNALS:
    void newCookie(const QNetworkCookie& cookie);
    void invalidatedAndFetched(const QByteArray& cookieData);

private:
    void storeShibCookie(const QNetworkCookie &cookie, Account *account);
    QUrl _url;
    QByteArray prepareCookieData() const;
    void disposeBrowser();

    QNetworkCookie _shibCookie;
    bool _ready;
    ShibbolethWebView* _browser;
    QMap<QUrl, QList<QNetworkCookie> > _otherCookies;
};

} // ns Mirall

#endif
