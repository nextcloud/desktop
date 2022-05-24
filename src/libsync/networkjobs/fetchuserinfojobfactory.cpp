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

#include "fetchuserinfojobfactory.h"
#include "common/utility.h"
#include "creds/httpcredentials.h"

#include <QJsonParseError>
#include <QNetworkReply>
#include <QStringLiteral>

Q_LOGGING_CATEGORY(lcFetchUserInfoJob, "sync.networkjob.fetchuserinfojob", QtInfoMsg);

namespace OCC {

FetchUserInfoJobFactory FetchUserInfoJobFactory::fromBasicAuthCredentials(QNetworkAccessManager *nam, const QString &username, const QString &password, QObject *parent)
{
    QString authorizationHeader = QStringLiteral("Basic %1").arg(QString::fromLocal8Bit(QStringLiteral("%1:%2").arg(username, password).toLocal8Bit().toBase64()));
    return { nam, authorizationHeader, parent };
}

FetchUserInfoJobFactory FetchUserInfoJobFactory::fromOAuth2Credentials(QNetworkAccessManager *nam, const QString &bearerToken, QObject *parent)
{
    QString authorizationHeader = QStringLiteral("Bearer %1").arg(bearerToken);
    return { nam, authorizationHeader, parent };
}

FetchUserInfoJobFactory::FetchUserInfoJobFactory(QNetworkAccessManager *nam, const QString &authHeaderValue, QObject *parent)
    : AbstractCoreJobFactory(nam, parent)
    , _authorizationHeader(authHeaderValue)
{
}

CoreJob *FetchUserInfoJobFactory::startJob(const QUrl &url)
{
    auto *job = new CoreJob;

    QUrlQuery urlQuery({ { QStringLiteral("format"), QStringLiteral("json") } });

    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("ocs/v2.php/cloud/user"), urlQuery));

    // We are not connected yet so we need to handle the authentication manually
    req.setRawHeader("Authorization", _authorizationHeader.toUtf8());
    req.setRawHeader(QByteArrayLiteral("OCS-APIREQUEST"), QByteArrayLiteral("true"));

    // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto reply = nam()->get(req);

    connect(reply, &QNetworkReply::finished, job, [reply, job] {
        reply->deleteLater();

        const auto data = reply->readAll();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
            setJobError(job, tr("Failed to retrieve user info"), reply);
        } else {
            qCDebug(lcFetchUserInfoJob) << data;

            QJsonParseError error = {};
            const auto json = QJsonDocument::fromJson(data, &error);

            if (error.error == QJsonParseError::NoError) {
                const auto jsonData = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();

                FetchUserInfoResult result(jsonData.value(QStringLiteral("id")).toString(), jsonData.value(QStringLiteral("display-name")).toString());

                setJobResult(job, QVariant::fromValue(result));
            } else {
                setJobError(job, error.errorString(), reply);
            }
        }
    });

    return job;
}

} // OCC
