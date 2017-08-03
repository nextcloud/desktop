/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "ocsjob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

Q_LOGGING_CATEGORY(lcOcs, "gui.sharing.ocs", QtInfoMsg)

OcsJob::OcsJob(AccountPtr account)
    : AbstractNetworkJob(account, "")
{
    _passStatusCodes.append(OCS_SUCCESS_STATUS_CODE);
    _passStatusCodes.append(OCS_SUCCESS_STATUS_CODE_V2);
    setIgnoreCredentialFailure(true);
}

void OcsJob::setVerb(const QByteArray &verb)
{
    _verb = verb;
}

void OcsJob::addParam(const QString &name, const QString &value)
{
    _params.append(qMakePair(name, value));
}

void OcsJob::addPassStatusCode(int code)
{
    _passStatusCodes.append(code);
}

void OcsJob::appendPath(const QString &id)
{
    setPath(path() + QLatin1Char('/') + id);
}

static QList<QPair<QByteArray, QByteArray>>
percentEncodeQueryItems(
    const QList<QPair<QString, QString>> &items)
{
    QList<QPair<QByteArray, QByteArray>> result;
    foreach (const auto &item, items) {
        result.append(qMakePair(
            QUrl::toPercentEncoding(item.first),
            QUrl::toPercentEncoding(item.second)));
    }
    return result;
}

void OcsJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Ocs-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QBuffer *buffer = new QBuffer;

    if (_verb == "GET") {
        // Note: QUrl::setQueryItems() does not fully percent encode
        // the query items, see #5042
        url.setEncodedQueryItems(percentEncodeQueryItems(_params));
    } else if (_verb == "POST" || _verb == "PUT") {
        // Url encode the _postParams and put them in a buffer.
        QByteArray postData;
        Q_FOREACH (auto tmp, _params) {
            if (!postData.isEmpty()) {
                postData.append("&");
            }
            postData.append(QUrl::toPercentEncoding(tmp.first));
            postData.append("=");
            postData.append(QUrl::toPercentEncoding(tmp.second));
        }
        buffer->setData(postData);
    }

    //We want json data
    auto queryItems = url.encodedQueryItems();
    queryItems.append(qMakePair(QByteArray("format"), QByteArray("json")));
    url.setEncodedQueryItems(queryItems);

    sendRequest(_verb, url, req, buffer);
    AbstractNetworkJob::start();
}

bool OcsJob::finished()
{
    const QByteArray replyData = reply()->readAll();

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(replyData, &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(lcOcs) << "Could not parse reply to"
                         << _verb
                         << Utility::concatUrlPath(account()->url(), path())
                         << _params
                         << error.errorString()
                         << ":" << replyData;
    }

    QString message;
    const int statusCode = getJsonReturnCode(json, message);
    if (!_passStatusCodes.contains(statusCode)) {
        qCWarning(lcOcs) << "Reply to"
                         << _verb
                         << Utility::concatUrlPath(account()->url(), path())
                         << _params
                         << "has unexpected status code:" << statusCode << replyData;
        emit ocsError(statusCode, message);
    } else {
        emit jobFinished(json);
    }
    return true;
}

int OcsJob::getJsonReturnCode(const QJsonDocument &json, QString &message)
{
    //TODO proper checking
    auto meta = json.object().value("ocs").toObject().value("meta").toObject();
    int code = meta.value("statuscode").toInt();
    message = meta.value("message").toString();

    return code;
}
}
