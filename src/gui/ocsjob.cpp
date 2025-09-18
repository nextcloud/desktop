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
    _params.insert(name, value);
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
    return _params.value(key);
}

static QUrlQuery percentEncodeQueryItems(
    const QHash<QString, QString> &items)
{
    QUrlQuery result;
    // Note: QUrlQuery::setQueryItems() does not fully percent encode
    // the query items, see #5042
    for (auto it = std::cbegin(items); it != std::cend(items); ++it) {
        result.addQueryItem(
            QUrl::toPercentEncoding(it.key()),
            QUrl::toPercentEncoding(it.value()));
    }
    return result;
}

void OcsJob::start()
{
    addRawHeader("Ocs-APIREQUEST", "true");
    addRawHeader("Content-Type", "application/x-www-form-urlencoded");

    auto *buffer = new QBuffer;

    QUrlQuery queryItems;
    if (_verb == "GET") {
        queryItems = percentEncodeQueryItems(_params);
    } else if (_verb == "POST" || _verb == "PUT") {
        // Url encode the _postParams and put them in a buffer.
        QByteArray postData;
        for (auto it = std::cbegin(_params); it != std::cend(_params); ++it) {
            if (!postData.isEmpty()) {
                postData.append("&");
            }
            postData.append(QUrl::toPercentEncoding(it.key()));
            postData.append("=");
            postData.append(QUrl::toPercentEncoding(it.value()));
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
        emit ocsError(statusCode, message);

    } else {
        // save new ETag value
        if (const auto etagHeader = reply()->header(QNetworkRequest::ETagHeader); etagHeader.isValid()) {
            emit etagResponseHeaderReceived(etagHeader.toByteArray(), statusCode);
        }

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
