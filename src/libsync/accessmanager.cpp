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
#include "httplogger.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcAccessManager, "sync.accessmanager", QtInfoMsg)

AccessManager::AccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
#ifndef Q_OS_LINUX
    // Atempt to workaround for https://github.com/owncloud/client/issues/3969
    setConfiguration(QNetworkConfiguration());
#endif
    setCookieJar(new CookieJar);
}

QByteArray AccessManager::generateRequestId()
{
    return QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
}

QNetworkReply *AccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkRequest newRequest(request);
    newRequest.setRawHeader(QByteArrayLiteral("User-Agent"), Utility::userAgentString());

    // Some firewalls reject requests that have a "User-Agent" but no "Accept" header
    newRequest.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));

    QByteArray verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    // For PROPFIND (assumed to be a WebDAV op), set xml/utf8 as content type/encoding
    // This needs extension
    if (verb == QByteArrayLiteral("PROPFIND")) {
        newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("text/xml; charset=utf-8"));
    }

    // Generate a new request id
    const QByteArray requestId = generateRequestId();
    qInfo(lcAccessManager) << op << verb << newRequest.url().toString() << "has X-Request-ID" << requestId;
    newRequest.setRawHeader(QByteArrayLiteral("X-Request-ID"), requestId);
    const auto originalIdKey = QByteArrayLiteral("Original-Request-ID");
    if (!newRequest.hasRawHeader(originalIdKey)) {
        newRequest.setRawHeader(originalIdKey, requestId);
    }

    if (newRequest.url().scheme() == QLatin1String("https")) { // Not for "http": QTBUG-61397
        // http2 seems to cause issues, as with our recommended server setup we don't support http2, disable it by default for now
        static const bool http2EnabledEnv = qEnvironmentVariableIntValue("OWNCLOUD_HTTP2_ENABLED") == 1;

        newRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2EnabledEnv);
    }

    auto sslConfiguration = newRequest.sslConfiguration();

    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionTickets, false);
    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionSharing, false);
    sslConfiguration.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
    if (!_customTrustedCaCertificates.isEmpty()) {
        // for some reason, passing an empty list causes the default chain to be removed
        // this behavior does not match the documentation
        sslConfiguration.addCaCertificates({ _customTrustedCaCertificates.begin(), _customTrustedCaCertificates.end() });
    }
    newRequest.setSslConfiguration(sslConfiguration);

    const auto reply = QNetworkAccessManager::createRequest(op, newRequest, outgoingData);
    HttpLogger::logRequest(reply, op, outgoingData);
    return reply;
}

QSet<QSslCertificate> AccessManager::customTrustedCaCertificates()
{
    return _customTrustedCaCertificates;
}

void AccessManager::setCustomTrustedCaCertificates(const QSet<QSslCertificate> &certificates)
{
    _customTrustedCaCertificates = certificates;
    // we have to terminate the existing (cached) connection to make the access manager re-evaluate the certificate sent by the server
    clearConnectionCache();
}

void AccessManager::addCustomTrustedCaCertificates(const QList<QSslCertificate> &certificates)
{
    _customTrustedCaCertificates.unite({ certificates.begin(), certificates.end() });

    // we have to terminate the existing (cached) connection to make the access manager re-evaluate the certificate sent by the server
    clearConnectionCache();
}

} // namespace OCC
