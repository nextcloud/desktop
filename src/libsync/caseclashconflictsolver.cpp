/*
 * Copyright 2021 (c) Felix Weilbach <felix.weilbach@nextcloud.com>
 * Copyright 2022 (c) Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "caseclashconflictsolver.h"

#include "networkjobs.h"
#include "propagateremotemove.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "common/filesystembase.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

using namespace OCC;

Q_LOGGING_CATEGORY(lcCaseClashConflictSolver, "nextcloud.sync.caseclash.solver", QtInfoMsg)

CaseClashConflictSolver::CaseClashConflictSolver(const QString &targetFilePath,
                                                 const QString &conflictFilePath,
                                                 const QString &remotePath,
                                                 const QString &localPath,
                                                 AccountPtr account,
                                                 SyncJournalDb *journal,
                                                 QObject *parent)
    : QObject{parent}
    , _account(account)
    , _targetFilePath(targetFilePath)
    , _conflictFilePath(conflictFilePath)
    , _remotePath(remotePath)
    , _localPath(localPath)
    , _journal(journal)
{
#if !defined(QT_NO_DEBUG)
    QFileInfo targetFileInfo(_targetFilePath);
    Q_ASSERT(targetFileInfo.isAbsolute());
    Q_ASSERT(FileSystem::fileExists(_conflictFilePath));
#endif
}

bool CaseClashConflictSolver::allowedToRename() const
{
    return _allowedToRename;
}

QString CaseClashConflictSolver::errorString() const
{
    return _errorString;
}

void CaseClashConflictSolver::solveConflict(const QString &newFilename)
{
    _newFilename = newFilename;

    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(remoteNewFilename()));
    connect(propfindJob, &PropfindJob::result, this, &CaseClashConflictSolver::onRemoteDestinationFileAlreadyExists);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &CaseClashConflictSolver::onRemoteDestinationFileDoesNotExist);
    propfindJob->start();
}

void CaseClashConflictSolver::onRemoteDestinationFileAlreadyExists()
{
    _allowedToRename = false;
    emit allowedToRenameChanged();
    _errorString = tr("Cannot rename file because a file with the same name already exists on the server. Please pick another name.");
    emit errorStringChanged();
}

void CaseClashConflictSolver::onRemoteDestinationFileDoesNotExist()
{
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(remoteTargetFilePath()));
    connect(propfindJob, &PropfindJob::result, this, &CaseClashConflictSolver::onRemoteSourceFileAlreadyExists);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &CaseClashConflictSolver::onRemoteSourceFileDoesNotExist);
    propfindJob->start();
}

void CaseClashConflictSolver::onPropfindPermissionSuccess(const QVariantMap &values)
{
    onCheckIfAllowedToRenameComplete(values);
}

void CaseClashConflictSolver::onPropfindPermissionError(QNetworkReply *reply)
{
    onCheckIfAllowedToRenameComplete({}, reply);
}

void CaseClashConflictSolver::onRemoteSourceFileAlreadyExists()
{
    const auto remoteSource = QDir::cleanPath(remoteTargetFilePath());
    const auto remoteDestionation = QDir::cleanPath(_account->davUrl().path() + remoteNewFilename());
    qCInfo(lcCaseClashConflictSolver) << "rename case clashing file from" << remoteSource << "to" << remoteDestionation;
    const auto moveJob = new MoveJob(_account, remoteSource, remoteDestionation, this);
    connect(moveJob, &MoveJob::finishedSignal, this, &CaseClashConflictSolver::onMoveJobFinished);
    moveJob->start();
}

void CaseClashConflictSolver::onRemoteSourceFileDoesNotExist()
{
    Q_EMIT failed();
}

void CaseClashConflictSolver::onMoveJobFinished()
{
    const auto job = qobject_cast<MoveJob *>(sender());
    const auto error = job->reply()->error();

    if (error != QNetworkReply::NoError) {
        _errorString = tr("Could not rename file. Please make sure you are connected to the server.");
        emit errorStringChanged();

        emit failed();
        return;
    }

    qCInfo(lcCaseClashConflictSolver) << "going to delete case clash conflict record" << _targetFilePath;
    _journal->deleteCaseClashConflictByPathRecord(_targetFilePath);

    qCInfo(lcCaseClashConflictSolver) << "going to delete" << _conflictFilePath;
    FileSystem::remove(_conflictFilePath);

    Q_EMIT done();
}

QString CaseClashConflictSolver::remoteNewFilename() const
{
    if (_remotePath == QStringLiteral("/")) {
        qCDebug(lcCaseClashConflictSolver) << _newFilename << _remotePath << _newFilename;
        return _newFilename;
    } else {
        const auto result = QString{_remotePath + _newFilename};
        qCDebug(lcCaseClashConflictSolver) << result << _remotePath << _newFilename;
        return result;
    }
}

QString CaseClashConflictSolver::remoteTargetFilePath() const
{
    if (_remotePath == QStringLiteral("/")) {
        const auto result = QString{_targetFilePath.mid(_localPath.length())};
        return result;
    } else {
        const auto result = QString{_remotePath + _targetFilePath.mid(_localPath.length())};
        return result;
    }
}

void CaseClashConflictSolver::onCheckIfAllowedToRenameComplete(const QVariantMap &values, QNetworkReply *reply)
{
    constexpr auto CONTENT_NOT_FOUND_ERROR = 404;

    const auto isAllowedToRename = [](const RemotePermissions remotePermissions) {
        return remotePermissions.hasPermission(remotePermissions.CanRename)
            && remotePermissions.hasPermission(remotePermissions.CanMove);
    };

    if (values.contains("permissions") && !isAllowedToRename(RemotePermissions::fromServerString(values["permissions"].toString()))) {
        _allowedToRename = false;
        emit allowedToRenameChanged();
        _errorString = tr("You don't have the permission to rename this file. Please ask the author of the file to rename it.");
        emit errorStringChanged();

        return;
    } else if (reply && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != CONTENT_NOT_FOUND_ERROR) {
        _allowedToRename = false;
        emit allowedToRenameChanged();
        _errorString = tr("Failed to fetch permissions with error %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        emit errorStringChanged();

        return;
    }

    _allowedToRename = true;
    emit allowedToRenameChanged();

    const auto filePathFileInfo = QFileInfo(_newFilename);
    const auto fileName = filePathFileInfo.fileName();
    processLeadingOrTrailingSpacesError(fileName);
}

void CaseClashConflictSolver::processLeadingOrTrailingSpacesError(const QString &fileName)
{
    const auto hasLeadingSpaces = fileName.startsWith(QLatin1Char(' '));
    const auto hasTrailingSpaces = fileName.endsWith(QLatin1Char(' '));

    if (hasLeadingSpaces || hasTrailingSpaces) {
        if (hasLeadingSpaces && hasTrailingSpaces) {
            _errorString = tr("Filename contains leading and trailing spaces.");
            emit errorStringChanged();
        } else if (hasLeadingSpaces) {
            _errorString = tr("Filename contains leading spaces.");
            emit errorStringChanged();
        } else if (hasTrailingSpaces) {
            _errorString = tr("Filename contains trailing spaces.");
            emit errorStringChanged();
        }

        _allowedToRename = false;
        emit allowedToRenameChanged();

        return;
    }

    _allowedToRename = true;
    emit allowedToRenameChanged();
}

void CaseClashConflictSolver::checkIfAllowedToRename()
{
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(remoteTargetFilePath()));
    propfindJob->setProperties({"http://owncloud.org/ns:permissions", "http://nextcloud.org/ns:is-mount-root"});
    connect(propfindJob, &PropfindJob::result, this, &CaseClashConflictSolver::onPropfindPermissionSuccess);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &CaseClashConflictSolver::onPropfindPermissionError);
    propfindJob->start();
}
