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

#include "fileprovidereditlocallyjob.h"

#include <QLoggingCategory>

#include "editlocallymanager.h"
#include "networkjobs.h"
#include "systray.h"

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderEditLocallyJob, "nextcloud.gui.fileprovidereditlocally", QtInfoMsg)

FileProviderEditLocallyJob::FileProviderEditLocallyJob(const AccountStatePtr &accountState,
                                                       const QString &relPath,
                                                       QObject *const parent)
    : QObject(parent)
    , _accountState(accountState)
    , _relPath(relPath)
{
}

void FileProviderEditLocallyJob::start()
{
    if (_relPath.isEmpty() || !_accountState) {
        qCWarning(lcFileProviderEditLocallyJob) << "Could not start setup."
                                                << "relPath:" << _relPath
                                                << "accountState:" << _accountState;
        showError(tr("Could not start editing locally."), tr("An error occurred during setup."));
        return;
    }

    const auto relPathSplit = _relPath.split(QLatin1Char('/'));
    if (relPathSplit.isEmpty()) {
        showError(tr("Could not find a file for local editing."
                     "Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    const auto filename = relPathSplit.last();
    Systray::instance()->createEditFileLocallyLoadingDialog(filename);

    qCDebug(lcFileProviderEditLocallyJob) << "Getting file ocId for" << _relPath;

    const auto idJob = new PropfindJob(_accountState->account(), _relPath, this);
    idJob->setProperties({ QByteArrayLiteral("http://owncloud.org/ns:id") });
    connect(idJob, &PropfindJob::finishedWithError, this, &FileProviderEditLocallyJob::idGetError);
    connect(idJob, &PropfindJob::result, this, &FileProviderEditLocallyJob::idGetFinished);

    connect(this, &FileProviderEditLocallyJob::ocIdAcquired, 
            this, &FileProviderEditLocallyJob::openFileProviderFile);

    idJob->start();
}

void FileProviderEditLocallyJob::showError(const QString &message, 
                                           const QString &informativeText)
{
    Systray::instance()->destroyEditFileLocallyLoadingDialog();  
    EditLocallyManager::showError(message, informativeText);
    Q_EMIT error(message, informativeText);
}

void FileProviderEditLocallyJob::idGetError(const QNetworkReply * const reply)
{
    const auto errorString = reply == nullptr ? "Unknown error" : reply->errorString();
    qCWarning(lcFileProviderEditLocallyJob) << "Could not get file ocId." << errorString;
    showError(tr("Could not get file ID."), errorString);
}

void FileProviderEditLocallyJob::idGetFinished(const QVariantMap &data)
{
    const auto ocId = data.value("id").toString();
    if (ocId.isEmpty()) {
        qCWarning(lcFileProviderEditLocallyJob) << "Could not get file ocId.";
        showError(tr("Could not get file identifier."), tr("The file identifier is empty."));
        return;
    }

    qCDebug(lcFileProviderEditLocallyJob) << "Got file ocId for" << _relPath << ocId;
    emit ocIdAcquired(ocId);
}

} // namespace OCC::Mac
