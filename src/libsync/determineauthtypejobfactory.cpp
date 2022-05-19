/*
 * Copyright (C) Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "determineauthtypejobfactory.h"

#include "common/utility.h"
#include "creds/httpcredentials.h"
#include "theme.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDetermineAuthTypeJob, "sync.networkjob.determineauthtype2", QtInfoMsg);

using namespace OCC;

DetermineAuthTypeJobFactory::DetermineAuthTypeJobFactory(QNetworkAccessManager *nam, QObject *parent)
    : AbstractCoreJobFactory(nam, parent)
{
}

DetermineAuthTypeJobFactory::~DetermineAuthTypeJobFactory() = default;

CoreJob *DetermineAuthTypeJobFactory::startJob(const QUrl &url)
{
    auto job = new CoreJob;

    // we explicitly use a legacy dav path here
    auto req = makeRequest(Utility::concatUrlPath(url, Theme::instance()->webDavPath()));

    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto *reply = nam()->sendCustomRequest(req, "PROPFIND");

    connect(reply, &QNetworkReply::finished, job, [reply, job] {
        reply->deleteLater();

        switch (reply->error()) {
        case QNetworkReply::AuthenticationRequiredError:
            break;
        case QNetworkReply::NoError:
            setJobError(job, tr("Server did not ask for authorization"), reply);
            return;
        default:
            setJobError(job, tr("Failed to determine auth type: %1").arg(reply->errorString()), reply);
            return;
        }

        const auto authChallenge = reply->rawHeader(QByteArrayLiteral("WWW-Authenticate")).toLower();

        // we fall back to basic in any case
        if (authChallenge.contains(QByteArrayLiteral("bearer "))) {
            setJobResult(job, qVariantFromValue(AuthType::OAuth));
        } else {
            if (authChallenge.isEmpty()) {
                qCWarning(lcDetermineAuthTypeJob) << "Did not receive WWW-Authenticate reply to auth-test PROPFIND";
            }

            setJobResult(job, qVariantFromValue(AuthType::Basic));
        }

        qCInfo(lcDetermineAuthTypeJob) << "Auth type for" << reply->url() << "is" << job->result();
    });

    return job;
}
