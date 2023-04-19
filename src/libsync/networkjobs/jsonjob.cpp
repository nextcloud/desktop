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
#include "jsonjob.h"

#include "account.h"
#include "common/utility.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>

Q_LOGGING_CATEGORY(lcJsonApiJob, "sync.networkjob.jsonapi", QtInfoMsg)

using namespace OCC;

JsonJob::~JsonJob()
{
}

void JsonJob::finished()
{
    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcJsonApiJob) << "Network error: " << this << errorString();
    } else {
        parse(reply()->readAll());
    }
    SimpleNetworkJob::finished();
}

void JsonJob::parse(const QByteArray &data)
{
    const auto doc = QJsonDocument::fromJson(data, &_parseError);
    // empty or invalid response
    if (_parseError.error != QJsonParseError::NoError || doc.isNull()) {
        qCWarning(lcJsonApiJob) << "invalid JSON!" << data << _parseError.errorString();
    } else {
        _data = doc.object();
    }
}

const QJsonParseError &JsonJob::parseError() const
{
    return _parseError;
}

const QJsonObject &JsonJob::data() const
{
    return _data;
}


JsonApiJob::JsonApiJob(AccountPtr account, const QString &path, const QByteArray &verb, const UrlQuery &arguments, const QNetworkRequest &req, QObject *parent)
    : JsonJob(account, account->url(), path, verb, arguments, req, parent)
{
    _request.setRawHeader(QByteArrayLiteral("OCS-APIREQUEST"), QByteArrayLiteral("true"));

    auto q = query();
    // HACK: q will  be empty for POST, PUT, PATCH request as we utilise the body (see SimpleNetworkJob)
    // however the oc api requires the format header to be part of the url
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    setQuery(q);
}

JsonApiJob::JsonApiJob(AccountPtr account, const QString &path, const UrlQuery &arguments, const QNetworkRequest &req, QObject *parent)
    : JsonApiJob(account, path, QByteArrayLiteral("GET"), arguments, req, parent)
{
}

int JsonApiJob::ocsStatus() const
{
    return _ocsStatus;
}

void JsonApiJob::parse(const QByteArray &rawData)
{
    static const QRegularExpression rex(QStringLiteral("<statuscode>(\\d+)</statuscode>"));
    const auto match = rex.match(QString::fromUtf8(rawData));
    if (match.hasMatch()) {
        // this is a error message coming back from ocs.
        _ocsStatus = match.captured(1).toInt();
    } else {
        JsonJob::parse(rawData);
        // example: "{"ocs":{"meta":{"status":"ok","statuscode":100,"message":null},"data":{"version":{"major":8,"minor":"... (504)
        if (parseError().error == QJsonParseError::NoError) {
            if (data().contains(QLatin1String("ocs"))) {
                const auto meta = data().value(QLatin1String("ocs")).toObject().value(QLatin1String("meta")).toObject();
                _ocsStatus = meta.value(QLatin1String("statuscode")).toInt();
                _ocsMessage = meta.value(QLatin1String("message")).toString();
            } else {
                _ocsMessage = parseError().errorString();
            }
        }
    }
}
const QString &JsonApiJob::ocsMessage() const
{
    return _ocsMessage;
}
bool JsonApiJob::ocsSuccess() const
{
    // v1 api: 100
    // v2 api: 200 as in http
    return ocsStatus() == 100 || ocsStatus() == 200;
}
