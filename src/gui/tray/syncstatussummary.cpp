/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "syncstatussummary.h"
#include "accountfwd.h"
#include "folderman.h"
#include "navigationpanehelper.h"
#include "networkjobs.h"
#include "syncresult.h"
#include "tray/usermodel.h"

#include <theme.h>

namespace {

OCC::SyncResult::Status determineSyncStatus(const OCC::SyncResult &syncResult)
{
    const auto status = syncResult.status();

    if (status == OCC::SyncResult::Success || status == OCC::SyncResult::Problem) {
        if (syncResult.hasUnresolvedConflicts()) {
            return OCC::SyncResult::Problem;
        }
        return OCC::SyncResult::Success;
    } else if (status == OCC::SyncResult::SyncPrepare || status == OCC::SyncResult::Undefined) {
        return OCC::SyncResult::SyncRunning;
    }
    return status;
}
}

namespace OCC {

Q_LOGGING_CATEGORY(lcSyncStatusModel, "nextcloud.gui.syncstatusmodel", QtInfoMsg)

SyncStatusSummary::SyncStatusSummary(QObject *parent)
    : QObject(parent)
{
    const auto folderMan = FolderMan::instance();
    connect(folderMan, &FolderMan::folderListChanged, this, &SyncStatusSummary::onFolderListChanged);
    connect(folderMan, &FolderMan::folderSyncStateChange, this, &SyncStatusSummary::onFolderSyncStateChanged);
}

bool SyncStatusSummary::reloadNeeded(AccountState *accountState) const
{
    if (_accountState.data() == accountState) {
        return false;
    }
    return true;
}

void SyncStatusSummary::load()
{
    const auto currentUser = UserModel::instance()->currentUser();
    if (!currentUser) {
        return;
    }
    setAccountState(currentUser->accountState());
    clearFolderErrors();
    connectToFoldersProgress(FolderMan::instance()->map());
    initSyncState();
}

double SyncStatusSummary::syncProgress() const
{
    return _progress;
}

QUrl SyncStatusSummary::syncIcon() const
{
    return _syncIcon;
}

bool SyncStatusSummary::syncing() const
{
    return _isSyncing;
}

void SyncStatusSummary::onFolderListChanged(const OCC::Folder::Map &folderMap)
{
    connectToFoldersProgress(folderMap);
}

void SyncStatusSummary::markFolderAsError(const Folder *folder)
{
    _foldersWithErrors.insert(folder->alias());
}

void SyncStatusSummary::markFolderAsSuccess(const Folder *folder)
{
    _foldersWithErrors.erase(folder->alias());
}

bool SyncStatusSummary::folderErrors() const
{
    return _foldersWithErrors.size() != 0;
}

bool SyncStatusSummary::folderError(const Folder *folder) const
{
    return _foldersWithErrors.find(folder->alias()) != _foldersWithErrors.end();
}

void SyncStatusSummary::clearFolderErrors()
{
    _foldersWithErrors.clear();
}

void SyncStatusSummary::setSyncStateForFolder(const Folder *folder)
{
    if (_accountState && !_accountState->isConnected()) {
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Offline"));
        setSyncStatusDetailString("");
        setSyncIcon(Theme::instance()->folderOffline());
        return;
    }

    const auto state = determineSyncStatus(folder->syncResult());

    switch (state) {
    case SyncResult::Success:
    case SyncResult::SyncPrepare:
        // Success should only be shown if all folders were fine
        if (!folderErrors() || folderError(folder)) {
            setSyncing(false);
            setTotalFiles(0);
            setSyncStatusString(tr("All synced!"));
            setSyncStatusDetailString("");
            setSyncIcon(Theme::instance()->syncStatusOk());
            markFolderAsSuccess(folder);
        }
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Some files couldn't be synced!"));
        setSyncStatusDetailString(tr("See below for errors"));
        setSyncIcon(Theme::instance()->syncStatusError());
        markFolderAsError(folder);
        break;
    case SyncResult::SyncRunning:
    case SyncResult::NotYetStarted:
        setSyncing(true);
        if (totalFiles() <= 0) {
            setSyncStatusString(tr("Checking folder changes"));
        } else {
            setSyncStatusString(tr("Syncing changes"));
        }
        setSyncStatusDetailString("");
        setSyncIcon(Theme::instance()->syncStatusRunning());
        break;
    case SyncResult::Paused:
    case SyncResult::SyncAbortRequested:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Sync paused"));
        setSyncStatusDetailString("");
        setSyncIcon(Theme::instance()->syncStatusPause());
        break;
    case SyncResult::Problem:
    case SyncResult::Undefined:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Some files could not be synced!"));
        setSyncStatusDetailString(tr("See below for warnings"));
        setSyncIcon(Theme::instance()->syncStatusWarning());
        markFolderAsError(folder);
        break;
    }
}

void SyncStatusSummary::onFolderSyncStateChanged(const Folder *folder)
{
    if (!folder) {
        return;
    }

    if (!_accountState || folder->accountState() != _accountState.data()) {
        return;
    }

    setSyncStateForFolder(folder);
}

constexpr double calculateOverallPercent(
    qint64 totalFileCount, qint64 completedFile, qint64 totalSize, qint64 completedSize)
{
    int overallPercent = 0;
    if (totalFileCount > 0) {
        // Add one 'byte' for each file so the percentage is moving when deleting or renaming files
        overallPercent = qRound(double(completedSize + completedFile) / double(totalSize + totalFileCount) * 100.0);
    }
    overallPercent = qBound(0, overallPercent, 100);
    return overallPercent / 100.0;
}

void SyncStatusSummary::onFolderProgressInfo(const ProgressInfo &progress)
{
    const qint64 completedSize = progress.completedSize();
    const qint64 currentFile = progress.currentFile();
    const qint64 completedFile = progress.completedFiles();
    const qint64 totalSize = qMax(completedSize, progress.totalSize());
    const qint64 numFilesInProgress = qMax(currentFile, progress.totalFiles());

    if (_totalFiles <= 0 && numFilesInProgress > 0) {
        setSyncStatusString(tr("Syncing"));
    }

    setTotalFiles(numFilesInProgress);

    setSyncProgress(calculateOverallPercent(numFilesInProgress, completedFile, totalSize, completedSize));

    if (totalSize > 0) {
        const auto completedSizeString = Utility::octetsToString(completedSize);
        const auto totalSizeString = Utility::octetsToString(totalSize);

        if (progress.trustEta()) {
            setSyncStatusDetailString(
                tr("%1 of %2 · %3 left")
                    .arg(completedSizeString, totalSizeString)
                    .arg(Utility::durationToDescriptiveString1(progress.totalProgress().estimatedEta)));
        } else {
            setSyncStatusDetailString(tr("%1 of %2").arg(completedSizeString, totalSizeString));
        }
    }

    if (numFilesInProgress > 0) {
        setSyncStatusString(tr("Syncing file %1 of %2").arg(currentFile).arg(numFilesInProgress));
    }
}

void SyncStatusSummary::setSyncing(bool value)
{
    if (value == _isSyncing) {
        return;
    }

    _isSyncing = value;
    emit syncingChanged();
}

void SyncStatusSummary::setTotalFiles(const qint64 value)
{
    if (value != _totalFiles) {
        _totalFiles = value;
        emit totalFilesChanged();
    }
}

void SyncStatusSummary::setSyncProgress(double value)
{
    if (_progress == value) {
        return;
    }

    _progress = value;
    emit syncProgressChanged();
}

void SyncStatusSummary::setSyncStatusString(const QString &value)
{
    if (_syncStatusString == value) {
        return;
    }

    _syncStatusString = value;
    emit syncStatusStringChanged();
}

QString SyncStatusSummary::syncStatusString() const
{
    return _syncStatusString;
}

QString SyncStatusSummary::syncStatusDetailString() const
{
    return _syncStatusDetailString;
}

qint64 SyncStatusSummary::totalFiles() const
{
    return _totalFiles;
}

void SyncStatusSummary::setSyncIcon(const QUrl &value)
{
    if (_syncIcon == value) {
        return;
    }

    _syncIcon = value;
    emit syncIconChanged();
}

void SyncStatusSummary::setSyncStatusDetailString(const QString &value)
{
    if (_syncStatusDetailString == value) {
        return;
    }

    _syncStatusDetailString = value;
    emit syncStatusDetailStringChanged();
}

void SyncStatusSummary::connectToFoldersProgress(const Folder::Map &folderMap)
{
    for (const auto &folder : folderMap) {
        if (folder->accountState() == _accountState.data()) {
            connect(
                folder, &Folder::progressInfo, this, &SyncStatusSummary::onFolderProgressInfo, Qt::UniqueConnection);
        } else {
            disconnect(folder, &Folder::progressInfo, this, &SyncStatusSummary::onFolderProgressInfo);
        }
    }
}

void SyncStatusSummary::onIsConnectedChanged()
{
    setSyncStateToConnectedState();
}

void SyncStatusSummary::setSyncStateToConnectedState()
{
    setSyncing(false);
    setTotalFiles(0);
    setSyncStatusDetailString("");
    if (_accountState && !_accountState->isConnected()) {
        setSyncStatusString(tr("Offline"));
        setSyncIcon(Theme::instance()->folderOffline());
    } else {
        setSyncStatusString(tr("All synced!"));
        setSyncIcon(Theme::instance()->syncStatusOk());
    }
}

void SyncStatusSummary::setAccountState(AccountStatePtr accountState)
{
    if (!reloadNeeded(accountState.data())) {
        return;
    }
    if (_accountState) {
        disconnect(
            _accountState.data(), &AccountState::isConnectedChanged, this, &SyncStatusSummary::onIsConnectedChanged);
    }
    _accountState = accountState;
    connect(_accountState.data(), &AccountState::isConnectedChanged, this, &SyncStatusSummary::onIsConnectedChanged);
}

void SyncStatusSummary::initSyncState()
{
    auto syncStateFallbackNeeded = true;
    for (const auto &folder : FolderMan::instance()->map()) {
        onFolderSyncStateChanged(folder);
        syncStateFallbackNeeded = false;
    }

    if (syncStateFallbackNeeded) {
        setSyncStateToConnectedState();
    }
}
}
