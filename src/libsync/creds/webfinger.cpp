/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "webfinger.h"

#include "common/asserts.h"

#include "httpcredentials.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>


Q_LOGGING_CATEGORY(lcWebFinger, "sync.credentials.webfinger", QtInfoMsg)

namespace {

auto relId()
{
    return QStringLiteral("http://webfinger.owncloud/rel/server-instance");
}
}

using namespace OCC;

WebFinger::WebFinger(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , _nam(nam)
{
}

void WebFinger::start(const QUrl &url, const QString &resourceId)
{
    //    GET /.well-known/webfinger?rel=http://webfinger.owncloud/rel/server-instance&resource=acct:test@owncloud.com HTTP/1.1
    if (OC_ENSURE(url.scheme() == QLatin1String("https"))) {
        QUrlQuery query;
        query.setQueryItems({ { QStringLiteral("resource"), QString::fromUtf8(QUrl::toPercentEncoding(QStringLiteral("acct:") + resourceId)) },
            { QStringLiteral("rel"), relId() } });

        QNetworkRequest req;
        req.setUrl(Utility::concatUrlPath(url, QStringLiteral(".well-known/webfinger"), query));
        req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

        auto *reply = _nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, this] {
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 200) {
                const auto data = reply->readAll();
                auto obj = QJsonDocument::fromJson(data, &_error).object();
                if (_error.error == QJsonParseError::NoError) {
                    const auto links = obj.value(QLatin1String("links")).toArray();
                    if (!links.empty()) {
                        _href = QUrl::fromEncoded(links.first().toObject().value(QLatin1String("href")).toString().toUtf8());
                        qCInfo(lcWebFinger) << "Webfinger provided" << _href << "as server";
                    } else {
                        qCWarning(lcWebFinger) << reply->url() << "Did not reply a valid link";
                    }
                } else {
                    qCWarning(lcWebFinger) << "Failed with" << _error.errorString();
                }
            } else {
                qCWarning(lcWebFinger) << "Failed with status code" << status;
                _error.error = QJsonParseError::MissingObject;
            }
            Q_EMIT finished();
        });
    } else {
        Q_EMIT finished();
    }
}

const QJsonParseError &WebFinger::error() const
{
    return _error;
}

const QUrl &WebFinger::href() const
{
    return _href;
}
