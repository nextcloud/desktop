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

#include <QDebug>
#include <QNetworkRequest>

#include "creds/shibboleth/shibbolethaccessmanager.h"

namespace Mirall
{

ShibbolethAccessManager::ShibbolethAccessManager(const QNetworkCookie& cookie, QObject* parent)
    : QNetworkAccessManager (parent),
      _cookie(cookie)
{}

QNetworkReply* ShibbolethAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    if (!_cookie.name().isEmpty()) {
        QNetworkCookieJar* jar(cookieJar());
        QUrl url(request.url());
        QList<QNetworkCookie> cookies;

        Q_FOREACH(const QNetworkCookie& cookie, jar->cookiesForUrl(url)) {
            if (!cookie.name().startsWith("_shibsession_")) {
                cookies << cookie;
            }
        }

        cookies << _cookie;
        jar->setCookiesFromUrl(cookies, url);
    }

    qDebug() << "Creating a request to " << request.url().toString() << " with shibboleth cookie:" << _cookie.name();

    return QNetworkAccessManager::createRequest (op, request, outgoingData);
}

void ShibbolethAccessManager::setCookie(const QNetworkCookie& cookie)
{
    qDebug() << "Got new shibboleth cookie:" << cookie.name();
    _cookie = cookie;
}

} // ns Mirall
