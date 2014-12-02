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

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QSslConfiguration>
#include <QNetworkCookie>
#include <QNetworkCookieJar>

#ifndef TOKEN_AUTH_ONLY
#include "authenticationdialog.h"
#endif

#include "cookiejar.h"
#include "accessmanager.h"
#include "utility.h"

namespace OCC
{

AccessManager::AccessManager(QObject* parent)
    : QNetworkAccessManager (parent)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) && defined(Q_OS_MAC)
    // FIXME Workaround http://stackoverflow.com/a/15707366/2941 https://bugreports.qt-project.org/browse/QTBUG-30434
    QNetworkProxy proxy = this->proxy();
    proxy.setHostName(" ");
    setProxy(proxy);
#endif
    setCookieJar(new CookieJar);
    connect(this, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
            this, SLOT(slotProxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
    connect(this, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));

}

void AccessManager::setRawCookie(const QByteArray &rawCookie, const  QUrl &url)
{
    QNetworkCookie cookie(rawCookie.left(rawCookie.indexOf('=')),
                          rawCookie.mid(rawCookie.indexOf('=')+1));
    qDebug() << Q_FUNC_INFO << cookie.name() << cookie.value();
    QList<QNetworkCookie> cookieList;
    cookieList.append(cookie);

    QNetworkCookieJar *jar = cookieJar();
    jar->setCookiesFromUrl(cookieList, url);
}

QNetworkReply* AccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    QNetworkRequest newRequest(request);

    if (newRequest.hasRawHeader("cookie")) {
        // This will set the cookie into the QNetworkCookieJar which will then override the cookie header
        setRawCookie(request.rawHeader("cookie"), request.url());
    }

    newRequest.setRawHeader(QByteArray("User-Agent"), Utility::userAgentString());
    QByteArray verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    // For PROPFIND (assumed to be a WebDAV op), set xml/utf8 as content type/encoding
    // This needs extension
    if (verb == "PROPFIND") {
        newRequest.setHeader( QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml; charset=utf-8"));
    }
    return QNetworkAccessManager::createRequest(op, newRequest, outgoingData);
}

void AccessManager::slotProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator)
{
    Q_UNUSED(authenticator);
    qDebug() << Q_FUNC_INFO << proxy.type();
    // We put in the password here and in ClientProxy in the proxy itself.
    if (!proxy.user().isEmpty() || !proxy.password().isEmpty()) {
        authenticator->setUser(proxy.user());
        authenticator->setPassword(proxy.password());
    }
}
void AccessManager::slotAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
#ifndef TOKEN_AUTH_ONLY
    // do not handle 401 created by the networkjobs. We may want
    // to eventually exempt some, but for now we need
    // it only for other things, e.g. the browser. Would we handle
    // network jobs, this would break the wizard logic
    if (reply->property("doNotHandleAuth").toBool()) {
        return;
    }
    QUrl url = reply->url();
    // show only scheme, host and port
    QUrl reducedUrl;
    reducedUrl.setScheme(url.scheme());
    reducedUrl.setHost(url.host());
    reducedUrl.setPort(url.port());

    AuthenticationDialog dialog(authenticator->realm(), reducedUrl.toString());
    if (dialog.exec() == QDialog::Accepted) {
        authenticator->setUser(dialog.user());
        authenticator->setPassword(dialog.password());
    }
#else
    Q_UNUSED(reply) Q_UNUSED(authenticator)
    Q_ASSERT(!"OCC::AccessManager::slotAuthenticationRequired called");
#endif
}

} // namespace OCC
