/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "determineuserjobfactory.h"
#include "common/utility.h"
#include "creds/httpcredentials.h"
#include <QJsonParseError>
#include <QNetworkReply>

Q_LOGGING_CATEGORY(lcDetermineUserJob, "sync.networkjob.determineuserjob", QtInfoMsg);

using namespace OCC;

DetermineUserJobFactory::DetermineUserJobFactory(QNetworkAccessManager *networkAccessManager, const QString &accessToken, QObject *parent)
    : AbstractCoreJobFactory(networkAccessManager, parent)
    , _accessToken(accessToken)
{
}

CoreJob *DetermineUserJobFactory::startJob(const QUrl &url)
{
    auto job = new CoreJob;

    QUrlQuery urlQuery({ { QStringLiteral("format"), QStringLiteral("json") } });

    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("ocs/v2.php/cloud/user"), urlQuery));

    // We are not connected yet so we need to handle the authentication manually
    req.setRawHeader("Authorization", "Bearer " + _accessToken.toUtf8());
    req.setRawHeader(QByteArrayLiteral("OCS-APIREQUEST"), QByteArrayLiteral("true"));
    // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto reply = nam()->get(req);

    connect(reply, &QNetworkReply::finished, job, [reply, job] {
        reply->deleteLater();

        const auto data = reply->readAll();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        qCDebug(lcDetermineUserJob) << "reply" << reply << "error" << reply->error() << "status code" << statusCode;
        qCDebug(lcDetermineUserJob) << "data:" << data;

        if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
            setJobError(job, tr("Failed to retrieve user info"), reply);
        } else {
            qCWarning(lcDetermineUserJob) << data;

            QJsonParseError error = {};
            const auto json = QJsonDocument::fromJson(data, &error);

            if (error.error == QJsonParseError::NoError) {
                const QString user = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toString();
                setJobResult(job, user);
            } else {
                setJobError(job, error.errorString(), reply);
            }
        }
    });

    return job;
}
