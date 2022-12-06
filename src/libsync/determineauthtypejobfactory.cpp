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

DetermineAuthTypeJobFactory::DetermineAuthTypeJobFactory(QNetworkAccessManager *nam)
    : AbstractCoreJobFactory(nam)
{
}

DetermineAuthTypeJobFactory::~DetermineAuthTypeJobFactory() = default;

CoreJob *DetermineAuthTypeJobFactory::startJob(const QUrl &url, QObject *parent)
{
    // we explicitly use a legacy dav path here
    auto req = makeRequest(Utility::concatUrlPath(url, Theme::instance()->webDavPath()));

    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    auto job = new CoreJob(nam()->sendCustomRequest(req, "PROPFIND"), parent);

    connect(job->reply(), &QNetworkReply::finished, job, [job] {
        switch (job->reply()->error()) {
        case QNetworkReply::AuthenticationRequiredError:
            break;
        case QNetworkReply::NoError:
            setJobError(job, tr("Server did not ask for authorization"));
            return;
        default:
            setJobError(job, tr("Failed to determine auth type: %1").arg(job->reply()->errorString()));
            return;
        }

        const auto authChallenge = job->reply()->rawHeader(QByteArrayLiteral("WWW-Authenticate")).toLower();

        const AuthType authType = [authChallenge]() {
            // we fall back to basic in any case
            if (authChallenge.contains(QByteArrayLiteral("bearer "))) {
                return AuthType::OAuth;
            } else {
                if (authChallenge.isEmpty()) {
                    qCWarning(lcDetermineAuthTypeJob) << "Did not receive WWW-Authenticate reply to auth-test PROPFIND";
                }

                return AuthType::Basic;
            }
        }();

        qCInfo(lcDetermineAuthTypeJob) << "Auth type for" << job->reply()->url() << "is" << authType;
        setJobResult(job, qVariantFromValue(authType));
    });

    return job;
}
