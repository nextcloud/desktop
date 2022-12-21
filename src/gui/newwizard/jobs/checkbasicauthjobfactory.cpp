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

#include "checkbasicauthjobfactory.h"

#include "accessmanager.h"
#include "common/utility.h"
#include "creds/httpcredentials.h"
#include "gui/tlserrordialog.h"
#include "gui/updateurldialog.h"
#include "theme.h"

#include <QNetworkReply>

namespace OCC::Wizard::Jobs {

CoreJob *CheckBasicAuthJobFactory::startJob(const QUrl &url, QObject *parent)
{
    QNetworkRequest req(Utility::concatUrlPath(url, Theme::instance()->webDavPath()));

    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    QString authorizationHeader = QStringLiteral("Basic %1").arg(QString::fromLocal8Bit(QStringLiteral("%1:%2").arg(_username, _password).toLocal8Bit().toBase64()));
    req.setRawHeader("Authorization", authorizationHeader.toLocal8Bit());

    auto *job = new CoreJob(nam()->sendCustomRequest(req, "PROPFIND"), parent);

    connect(job->reply(), &QNetworkReply::finished, job, [job]() {
        switch (job->reply()->error()) {
        case QNetworkReply::NoError:
            setJobResult(job, true);
            return;
        case QNetworkReply::AuthenticationRequiredError:
            if (OC_ENSURE(job->reply()->rawHeader(QByteArrayLiteral("WWW-Authenticate")).toLower().contains("basic "))) {
                setJobResult(job, false);
                return;
            }
            Q_FALLTHROUGH();
        default:
            setJobError(job, tr("Invalid reply received from server"));
            return;
        }
    });

    makeRequest();

    return job;
}

CheckBasicAuthJobFactory::CheckBasicAuthJobFactory(QNetworkAccessManager *nam, const QString &username, const QString &password)
    : AbstractCoreJobFactory(nam)
    , _username(username)
    , _password(password)
{
}

}
