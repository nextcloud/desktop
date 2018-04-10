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

Q_LOGGING_CATEGORY(lcOcs, "nextcloud.gui.sharing.ocs", QtInfoMsg)

OcsJob::OcsJob(AccountPtr account)
    : AbstractNetworkJob(account, "")
{
    _passStatusCodes.append(OCS_SUCCESS_STATUS_CODE);
    _passStatusCodes.append(OCS_SUCCESS_STATUS_CODE_V2);
    _passStatusCodes.append(OCS_NOT_MODIFIED_STATUS_CODE_V2);
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

void OcsJob::addRawHeader(const QByteArray &headerName, const QByteArray &value)
{
    _request.setRawHeader(headerName, value);
}

static QUrlQuery percentEncodeQueryItems(
    const QList<QPair<QString, QString>> &items)
{
    QUrlQuery result;
    // Note: QUrlQuery::setQueryItems() does not fully percent encode
    // the query items, see #5042
    foreach (const auto &item, items) {
        result.addQueryItem(
            QUrl::toPercentEncoding(item.first),
            QUrl::toPercentEncoding(item.second));
    }
    return result;
}

void OcsJob::start()
{
    addRawHeader("Ocs-APIREQUEST", "true");
    addRawHeader("Content-Type", "application/x-www-form-urlencoded");

    QBuffer *buffer = new QBuffer;

    QUrlQuery queryItems;
    if (_verb == "GET") {
        queryItems = percentEncodeQueryItems(_params);
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
    queryItems.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path(), queryItems);
    sendRequest(_verb, url, _request, buffer);
    AbstractNetworkJob::start();
}

bool OcsJob::finished()
{
    const QByteArray replyData = reply()->readAll();

    QJsonParseError error;
    QString message;
    int statusCode = 0;
    auto json = QJsonDocument::fromJson(replyData, &error);

    // when it is null we might have a 304 so get status code from reply() and gives a warning...
    if (error.error != QJsonParseError::NoError) {
        statusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCWarning(lcOcs) << "Could not parse reply to"
                         << _verb
                         << Utility::concatUrlPath(account()->url(), path())
                         << _params
                         << error.errorString()
                         << ":" << replyData;
    } else {
        statusCode  = getJsonReturnCode(json, message);
    }

    //... then it checks for the statusCode
    if (!_passStatusCodes.contains(statusCode)) {
        qCWarning(lcOcs) << "Reply to"
                         << _verb
                         << Utility::concatUrlPath(account()->url(), path())
                         << _params
                         << "has unexpected status code:" << statusCode << replyData;
        emit ocsError(statusCode, message);

    } else {
        // save new ETag value
        if(reply()->rawHeaderList().contains("ETag"))
            emit etagResponseHeaderReceived(reply()->rawHeader("ETag"), statusCode);

        emit jobFinished(json, statusCode);
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
