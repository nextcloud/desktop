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

#include "webfingeruserinfojobfactory.h"
#include "common/utility.h"
#include "creds/httpcredentials.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QStringLiteral>

Q_LOGGING_CATEGORY(lcWebFingerUserInfoJob, "sync.networkjob.webfingeruserinfojob", QtInfoMsg);

namespace OCC::Wizard::Jobs {

WebFingerInstanceLookupJobFactory::WebFingerInstanceLookupJobFactory(QNetworkAccessManager *nam, const QString &bearerToken)
    : AbstractCoreJobFactory(nam)
    , _authorizationHeader(QStringLiteral("Bearer %1").arg(bearerToken))
{
}

CoreJob *WebFingerInstanceLookupJobFactory::startJob(const QUrl &url, QObject *parent)
{
    QUrlQuery urlQuery({{QStringLiteral("format"), QStringLiteral("json")}});

    const QString resource = QStringLiteral("acct:me@%1").arg(url.host());

    // TODO: acct:me@host
    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("/.well-known/webfinger"), {{QStringLiteral("resource"), resource}}));

    // we are not connected yet, so we need to handle the authentication manually
    req.setRawHeader("Authorization", _authorizationHeader.toUtf8());

    // we just added the Authorization header, don't let HttpCredentialsAccessManager tamper with it
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto *job = new CoreJob(nam()->get(req), parent);

    QObject::connect(job->reply(), &QNetworkReply::finished, job, [job, resource] {
        const auto data = job->reply()->readAll();
        qCDebug(lcWebFingerUserInfoJob) << data;
        const auto statusCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (job->reply()->error() != QNetworkReply::NoError || statusCode != 200) {
            setJobError(job, QApplication::translate("WebFingerUserInfoJobFactory", "Failed to retrieve user info"));
            return;
        }

        QJsonParseError error = {};
        const auto json = QJsonDocument::fromJson(data, &error);

        if (error.error != QJsonParseError::NoError) {
            setJobError(job, error.errorString());
            return;
        }

        qCDebug(lcWebFingerUserInfoJob) << "retrieved instances list for user" << json.object().value(QStringLiteral("subject")).toString();

        const auto links = json.object().value(QStringLiteral("links")).toArray();

        qCDebug(lcWebFingerUserInfoJob) << "found links:" << links;

        // we only intend to return server instance(s) currently, additional information is discarded
        QVector<QUrl> instanceUrls;

        for (const auto &link : links) {
            const auto linkObject = link.toObject();

            const QString rel = linkObject.value(QStringLiteral("rel")).toString();
            const QString href = linkObject.value(QStringLiteral("href")).toString();

            if (rel != QStringLiteral("http://webfinger.owncloud/rel/server-instance")) {
                qCDebug(lcWebFingerUserInfoJob) << "skipping invalid link" << href << "with rel" << rel;
                continue;
            }

            instanceUrls.append(QUrl::fromUserInput(href));
        }

        setJobResult(job, QVariant::fromValue(instanceUrls));
    });

    return job;
}

} // OCC
