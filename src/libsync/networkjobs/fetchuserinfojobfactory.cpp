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

FetchUserInfoJobFactory FetchUserInfoJobFactory::fromBasicAuthCredentials(QNetworkAccessManager *nam, const QString &username, const QString &password)
{
    QString authorizationHeader = QStringLiteral("Basic %1").arg(QString::fromUtf8(QStringLiteral("%1:%2").arg(username, password).toUtf8().toBase64()));
    return { nam, authorizationHeader };
}

FetchUserInfoJobFactory FetchUserInfoJobFactory::fromOAuth2Credentials(QNetworkAccessManager *nam, const QString &bearerToken)
{
    QString authorizationHeader = QStringLiteral("Bearer %1").arg(bearerToken);
    return { nam, authorizationHeader };
}

FetchUserInfoJobFactory::FetchUserInfoJobFactory(QNetworkAccessManager *nam, const QString &authHeaderValue)
    : AbstractCoreJobFactory(nam)
    , _authorizationHeader(authHeaderValue)
{
}

CoreJob *FetchUserInfoJobFactory::startJob(const QUrl &url, QObject *parent)
{
    QUrlQuery urlQuery({ { QStringLiteral("format"), QStringLiteral("json") } });

    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("ocs/v2.php/cloud/user"), urlQuery));

    // We are not connected yet so we need to handle the authentication manually
    req.setRawHeader("Authorization", _authorizationHeader.toUtf8());
    req.setRawHeader(QByteArrayLiteral("OCS-APIREQUEST"), QByteArrayLiteral("true"));

    // We just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto *job = new CoreJob(nam()->get(req), parent);

    connect(job->reply(), &QNetworkReply::finished, job, [job] {
        const auto data = job->reply()->readAll();
        const auto statusCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (job->reply()->error() != QNetworkReply::NoError || statusCode != 200) {
            setJobError(job, tr("Failed to retrieve user info"));
        } else {
            qCDebug(lcFetchUserInfoJob) << data;

            QJsonParseError error = {};
            const auto json = QJsonDocument::fromJson(data, &error);

            if (error.error == QJsonParseError::NoError) {
                const auto jsonData = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();

                FetchUserInfoResult result(jsonData.value(QStringLiteral("id")).toString(), jsonData.value(QStringLiteral("display-name")).toString());

                setJobResult(job, QVariant::fromValue(result));
            } else {
                setJobError(job, error.errorString());
            }
        }
    });

    return job;
}

} // OCC
