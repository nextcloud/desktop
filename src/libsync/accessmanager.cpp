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

#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QAuthenticator>
#include <QSslConfiguration>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkConfiguration>
#include <QUuid>

#include "cookiejar.h"
#include "accessmanager.h"
#include "common/utility.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcAccessManager, "sync.accessmanager", QtInfoMsg)

AccessManager::AccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
#if defined(Q_OS_MAC)
    // FIXME Workaround http://stackoverflow.com/a/15707366/2941 https://bugreports.qt-project.org/browse/QTBUG-30434
    QNetworkProxy proxy = this->proxy();
    proxy.setHostName(" ");
    setProxy(proxy);
#endif

#ifndef Q_OS_LINUX
    // Atempt to workaround for https://github.com/owncloud/client/issues/3969
    setConfiguration(QNetworkConfiguration());
#endif
    setCookieJar(new CookieJar);
}

void AccessManager::setRawCookie(const QByteArray &rawCookie, const QUrl &url)
{
    QNetworkCookie cookie(rawCookie.left(rawCookie.indexOf('=')),
        rawCookie.mid(rawCookie.indexOf('=') + 1));
    qCDebug(lcAccessManager) << cookie.name() << cookie.value();
    QList<QNetworkCookie> cookieList;
    cookieList.append(cookie);

    QNetworkCookieJar *jar = cookieJar();
    jar->setCookiesFromUrl(cookieList, url);
}

static QByteArray generateRequestId()
{
    // Use a UUID with the starting and ending curly brace removed.
    auto uuid = QUuid::createUuid().toByteArray();
    return uuid.mid(1, uuid.size() - 2);
}

QNetworkReply *AccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkRequest newRequest(request);

    if (newRequest.hasRawHeader("cookie")) {
        // This will set the cookie into the QNetworkCookieJar which will then override the cookie header
        setRawCookie(request.rawHeader("cookie"), request.url());
    }

    newRequest.setRawHeader(QByteArray("User-Agent"), Utility::userAgentString());

    // Some firewalls reject requests that have a "User-Agent" but no "Accept" header
    newRequest.setRawHeader(QByteArray("Accept"), "*/*");

    QByteArray verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    // For PROPFIND (assumed to be a WebDAV op), set xml/utf8 as content type/encoding
    // This needs extension
    if (verb == "PROPFIND") {
        newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml; charset=utf-8"));
    }

    // Generate a new request id
    QByteArray requestId = generateRequestId();
    qInfo(lcAccessManager) << op << verb << newRequest.url().toString() << "has X-Request-ID" << requestId;
    newRequest.setRawHeader("X-Request-ID", requestId);

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 4)
    // only enable HTTP2 with Qt 5.9.4 because old Qt have too many bugs (e.g. QTBUG-64359 is fixed in >= Qt 5.9.4)
    if (newRequest.url().scheme() == "https") { // Not for "http": QTBUG-61397
        newRequest.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
    }
#endif

    return QNetworkAccessManager::createRequest(op, newRequest, outgoingData);
}

} // namespace OCC
