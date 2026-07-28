/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsjob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

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
    _params.emplaceBack(name, value);
}

void OcsJob::setJsonBody(const QJsonObject &body)
{
    _jsonBody = QJsonDocument{body}.toJson(QJsonDocument::Compact);
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

QString OcsJob::getParamValue(const QString &key) const
{
    const auto parameter = std::find_if(_params.cbegin(), _params.cend(), [&key](const auto &entry) {
        return entry.first == key;
    });
    return parameter == _params.cend() ? QString{} : parameter->second;
}

static QUrlQuery percentEncodeQueryItems(
    const QList<QPair<QString, QString>> &items)
{
    QUrlQuery result;
    // Note: QUrlQuery::setQueryItems() does not fully percent encode
    // the query items, see #5042
    for (const auto &[name, value] : items) {
        result.addQueryItem(
            QUrl::toPercentEncoding(name),
            QUrl::toPercentEncoding(value));
    }
    return result;
}

void OcsJob::start()
{
    addRawHeader("Ocs-APIREQUEST", "true");

    auto *buffer = new QBuffer;

    QUrlQuery queryItems;
    if (_verb == "GET" || _verb == "DELETE") {
        queryItems = percentEncodeQueryItems(_params);
    } else if (_verb == "POST" || _verb == "PUT") {
        if (_jsonBody.has_value()) {
            addRawHeader("Content-Type", "application/json");
            buffer->setData(*_jsonBody);
        } else {
            addRawHeader("Content-Type", "application/x-www-form-urlencoded");
        QByteArray postData;
            for (const auto &[name, value] : std::as_const(_params)) {
            if (!postData.isEmpty()) {
                postData.append("&");
            }
                postData.append(QUrl::toPercentEncoding(name));
            postData.append("=");
                postData.append(QUrl::toPercentEncoding(value));
        }
        buffer->setData(postData);
    }
    }
    queryItems.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path(), queryItems);
    sendRequest(_verb, url, _request, buffer);
    AbstractNetworkJob::start();
}

bool OcsJob::finished()
{
    const QByteArray replyData = reply()->readAll();

    QJsonParseError error{};
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
        Q_EMIT ocsError(statusCode, message);

    } else {
        // save new ETag value
        if (const auto etagHeader = reply()->header(QNetworkRequest::ETagHeader); etagHeader.isValid()) {
            Q_EMIT etagResponseHeaderReceived(etagHeader.toByteArray(), statusCode);
        }

        Q_EMIT jobFinished(json, statusCode);
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
