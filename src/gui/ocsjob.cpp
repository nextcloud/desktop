/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "ocsjob.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"

#include <QBuffer>

namespace OCC {

OCSJob::OCSJob(AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, "", parent)
{
    _passStatusCodes.append(100);
    setIgnoreCredentialFailure(true);
}

void OCSJob::setVerb(const QByteArray &verb)
{
    _verb = verb;
}

void OCSJob::setUrl(const QUrl &url)
{
    _url = url;
}

void OCSJob::setGetParams(const QList<QPair<QString, QString> >& getParams)
{
    _url.setQueryItems(getParams);
}

void OCSJob::setPostParams(const QList<QPair<QString, QString> >& postParams)
{
    _postParams = postParams;
}

void OCSJob::addPassStatusCode(int code)
{
    _passStatusCodes.append(code);
}

void OCSJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    // Url encode the _postParams and put them in a buffer.
    QByteArray postData;
    Q_FOREACH(auto tmp2, _postParams) {
        if (! postData.isEmpty()) {
            postData.append("&");
        }
        postData.append(QUrl::toPercentEncoding(tmp2.first));
        postData.append("=");
        postData.append(QUrl::toPercentEncoding(tmp2.second));
    }
    QBuffer *buffer = new QBuffer;
    buffer->setData(postData);

    auto queryItems = _url.queryItems();
    queryItems.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    _url.setQueryItems(queryItems);

    setReply(davRequest(_verb, _url, req, buffer));
    setupConnections(reply());
    buffer->setParent(reply());
    AbstractNetworkJob::start();
}

bool OCSJob::finished()
{
    const QString replyData = reply()->readAll();

    bool success;
    QVariantMap json = QtJson::parse(replyData, success).toMap();
    if (!success) {
        qDebug() << "Could not parse reply to" << _verb << _url << _postParams
                 << ":" << replyData;
    }

    QString message;
    const int statusCode = getJsonReturnCode(json, message);
    if (!_passStatusCodes.contains(statusCode)) {
        qDebug() << "Reply to" << _verb << _url << _postParams
                 << "has unexpected status code:" << statusCode << replyData;
    }

    emit jobFinished(json);
    return true;
}

int OCSJob::getJsonReturnCode(const QVariantMap &json, QString &message)
{
    //TODO proper checking
    int code = json.value("ocs").toMap().value("meta").toMap().value("statuscode").toInt();
    message = json.value("ocs").toMap().value("meta").toMap().value("message").toString();

    return code;
}

}
