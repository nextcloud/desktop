/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "editlocallyverificationjob.h"

#include <QLoggingCategory>
#include <QUrlQuery>

#include "libsync/networkjobs.h"

namespace OCC
{

Q_LOGGING_CATEGORY(lcEditLocallyVerificationJob, "nextcloud.gui.editlocallyverificationjob", QtInfoMsg)

EditLocallyVerificationJob::EditLocallyVerificationJob(const AccountStatePtr &accountState, 
                                                       const QString &relPath, 
                                                       const QString &token, 
                                                       QObject *const parent)
    : QObject(parent)
    , _accountState(accountState)
    , _relPath(relPath)
    , _token(token)
{
}

void EditLocallyVerificationJob::start()
{
    if (!_accountState || _relPath.isEmpty() || _token.isEmpty()) {
        qCWarning(lcEditLocallyVerificationJob) << "Could not start token check."
                                                << "accountState:" << _accountState 
                                                << "relPath:" << _relPath 
                                                << "token:" << _token;

        emit error(tr("Could not start editing locally."), 
                   tr("An error occurred trying to verify the request to edit locally."));
        return;
    }

    const auto encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(_token)); // Sanitise the token
    const auto encodedRelPath = QUrl::toPercentEncoding(_relPath); // Sanitise the relPath
    const auto checkTokenJob = new SimpleApiJob(_accountState->account(), 
                                                QStringLiteral("/ocs/v2.php/apps/files/api/v1/openlocaleditor/%1").arg(encodedToken));
    const auto slashedPath = encodedRelPath.startsWith('/') ? encodedRelPath : '/' + encodedRelPath;

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("path"), slashedPath);
    checkTokenJob->addQueryParams(params);
    checkTokenJob->setVerb(SimpleApiJob::Verb::Post);
    connect(checkTokenJob, &SimpleApiJob::resultReceived, this, &EditLocallyVerificationJob::responseReceived);

    checkTokenJob->start();
}

void EditLocallyVerificationJob::responseReceived(const int statusCode)
{
    if (statusCode == 200) {
        emit finished();
    } else {
        emit error(tr("Could not start editing locally."), 
                   tr("An error occurred trying to verify the request to edit locally."));
    }

    deleteLater();
}

}
