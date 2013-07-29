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

#include <QNetworkRequest>

#include "mirall/creds/shibbolethaccessmanager.h"

namespace Mirall
{

ShibbolethAccessManager::ShibbolethAccessManager (const QNetworkCookie& cookie, QObject* parent)
    : QNetworkAccessManager (parent),
      _cookie(cookie)
{}

QNetworkReply* ShibbolethAccessManager::createRequest (QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    QNetworkRequest newRequest(request);
    QList<QNetworkCookie> cookies(request.header(QNetworkRequest::CookieHeader).value< QList< QNetworkCookie > >());

    cookies << _cookie;
    newRequest.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue (cookies));

    return QNetworkAccessManager::createRequest (op, newRequest, outgoingData);
}

} // ns Mirall
