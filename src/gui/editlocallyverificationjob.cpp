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

#include <QDir>
#include <QLoggingCategory>
#include <QUrlQuery>

#include "libsync/networkjobs.h"

namespace {

QString prefixSlashToPath(const QString &path)
{
    return path.startsWith('/') ? path : '/' + path;
}

}

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

bool EditLocallyVerificationJob::isTokenValid(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    // Token is an alphanumeric string 128 chars long.
    // Ensure that is what we received and what we are sending to the server.
    static const QRegularExpression tokenRegex("^[a-zA-Z0-9]{128}$");
    const auto regexMatch = tokenRegex.match(token);

    return regexMatch.hasMatch();
}

bool EditLocallyVerificationJob::isRelPathValid(const QString &relPath)
{
    if (relPath.isEmpty()) {
        return false;
    }

    // We want to check that the path is canonical and not relative
    // (i.e. that it doesn't contain ../../) but we always receive
    // a relative path, so let's make it absolute by prepending a
    // slash
    const auto slashPrefixedPath = prefixSlashToPath(relPath);

    // Let's check that the filepath is canonical, and that the request
    // contains no funny behaviour regarding paths
    const auto cleanedPath = QDir::cleanPath(slashPrefixedPath);

    if (cleanedPath != slashPrefixedPath) {
        return false;
    }

    return true;
}

void EditLocallyVerificationJob::start()
{
    // We check the input data locally first, without modifying any state or
    // showing any potentially misleading data to the user
    if (!isTokenValid(_token)) {
        qCWarning(lcEditLocallyVerificationJob) << "Edit locally request is missing a valid token, will not open file. "
                                                << "Token received was:" << _token;
        emit error(tr("Invalid token received."), tr("Please try again."));
        return;
    }

    if (!isRelPathValid(_relPath)) {
        qCWarning(lcEditLocallyVerificationJob) << "Provided relPath was:" << _relPath 
                                                << "which is not canonical.";
        emit error(tr("Invalid file path was provided."), tr("Please try again."));
        return;
    }

    if (!_accountState) {
        qCWarning(lcEditLocallyVerificationJob) << "No account found to edit file " << _relPath << " locally.";
        emit error(tr("Could not find an account for local editing."), tr("Please try again."));
        return;
    }

    const auto encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(_token)); // Sanitise the token
    const auto encodedRelPath = QUrl::toPercentEncoding(_relPath); // Sanitise the relPath
    const auto checkTokenJob = new SimpleApiJob(_accountState->account(), 
                                                QStringLiteral("/ocs/v2.php/apps/files/api/v1/openlocaleditor/%1").arg(encodedToken));
    const auto slashedPath = prefixSlashToPath(encodedRelPath);

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
}

}
