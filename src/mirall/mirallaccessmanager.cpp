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
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QSslConfiguration>

#include "mirall/cookiejar.h"
#include "mirall/mirallaccessmanager.h"
#include "mirall/utility.h"

namespace Mirall
{

MirallAccessManager::MirallAccessManager(QObject* parent)
    : QNetworkAccessManager (parent)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) && defined(Q_OS_MAC)
    // FIXME Workaround http://stackoverflow.com/a/15707366/2941 https://bugreports.qt-project.org/browse/QTBUG-30434
    QNetworkProxy proxy = this->proxy();
    proxy.setHostName(" ");
    setProxy(proxy);
#endif
    setCookieJar(new CookieJar);
    QObject::connect(this, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
                     this, SLOT(slotProxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
}

QNetworkReply* MirallAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    QNetworkRequest newRequest(request);
    newRequest.setRawHeader(QByteArray("User-Agent"), Utility::userAgentString());
    QByteArray verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    // For PROPFIND (assumed to be a WebDAV op), set xml/utf8 as content type/encoding
    // This needs extension
    if (verb == "PROPFIND") {
        newRequest.setHeader( QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml; charset=utf-8"));
    }
    return QNetworkAccessManager::createRequest(op, newRequest, outgoingData);
}

void MirallAccessManager::slotProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator)
{
    Q_UNUSED(authenticator);
    qDebug() << Q_FUNC_INFO << proxy.type();
    // We put in the password here and in ClientProxy in the proxy itself.
    if (!proxy.user().isEmpty() || !proxy.password().isEmpty()) {
        authenticator->setUser(proxy.user());
        authenticator->setPassword(proxy.password());
    }
}

} // ns Mirall
