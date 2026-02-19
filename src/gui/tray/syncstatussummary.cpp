/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncstatussummary.h"
#include "accountfwd.h"
#include "accountstate.h"
#include "folderman.h"
#include "navigationpanehelper.h"
#include "networkjobs.h"
#include "syncresult.h"
#include "tray/usermodel.h"

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileproviderservice.h"
#include "gui/macOS/fileprovidersettingscontroller.h"
#endif

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

bool hasConfiguredSyncSource(const OCC::AccountStatePtr &accountState)
{
    if (!accountState) {
        return false;
    }

    for (const auto &folder : OCC::FolderMan::instance()->map()) {
        if (folder && folder->accountState() == accountState.data()) {
            return true;
        }
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    const auto account = accountState->account();
    const auto userIdAtHostWithPort = account->userIdAtHostWithPort();
    if (OCC::Mac::FileProviderSettingsController::instance()->vfsEnabledForAccount(userIdAtHostWithPort)) {
        return true;
    }
#endif

    return false;
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
#ifdef BUILD_FILE_PROVIDER_MODULE
    connect(Mac::FileProvider::instance()->service(), &Mac::FileProviderService::syncStateChanged, this, &SyncStatusSummary::onFileProviderDomainSyncStateChanged);
#endif
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
    return !_foldersWithErrors.empty();
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
        if (_accountState->state() == AccountState::NeedToSignTermsOfService) {
            setSyncStatusDetailString(tr("You need to accept the terms of service"));
        }
        setSyncIcon(Theme::instance()->folderOffline());
        return;
    }

    const auto state = determineSyncStatus(folder->syncResult());

    switch (state) {
    case SyncResult::Success:
    case SyncResult::SyncPrepare:
        markFolderAsSuccess(folder);
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
    case SyncResult::Problem:
    case SyncResult::Undefined:
        markFolderAsError(folder);
    case SyncResult::SyncRunning:
    case SyncResult::NotYetStarted:
    case SyncResult::Paused:
    case SyncResult::SyncAbortRequested:
        break;
    }

    setSyncState(state);
}

void SyncStatusSummary::setSyncState(const SyncResult::Status state)
{
    if (_accountState && !_accountState->isConnected()) {
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Offline"));
        setSyncStatusDetailString("");
        setSyncIcon(Theme::instance()->offline());
        return;
    }

    switch (state) {
    case SyncResult::Success:
    case SyncResult::SyncPrepare:
        // Success should only be shown if all folders were fine
        if (!folderErrors()
#ifdef BUILD_FILE_PROVIDER_MODULE
            && _fileProviderDomainsWithErrors.empty()
#endif
        ) {
            setSyncing(false);
            setTotalFiles(0);
            setSyncStatusString(tr("All synced!"));
            setSyncStatusDetailString("");
            setSyncIcon(Theme::instance()->ok());
        }
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Some files couldn't be synced!"));
        setSyncStatusDetailString(tr("See below for errors"));
        setSyncIcon(Theme::instance()->error());
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
        setSyncIcon(Theme::instance()->sync());
        break;
    case SyncResult::Paused:
    case SyncResult::SyncAbortRequested:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Sync paused"));
        setSyncStatusDetailString("");
        setSyncIcon(Theme::instance()->pause());
        break;
    case SyncResult::Problem:
    case SyncResult::Undefined:
        setSyncing(false);
        setTotalFiles(0);
        setSyncStatusString(tr("Some files could not be synced!"));
        setSyncStatusDetailString(tr("See below for warnings"));
        setSyncIcon(Theme::instance()->warning());
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

#ifdef BUILD_FILE_PROVIDER_MODULE
void SyncStatusSummary::onFileProviderDomainSyncStateChanged(const AccountPtr &account, SyncResult::Status state)
{
    if (!_accountState || !_accountState->isConnected() || account != _accountState->account()) {
        return;
    }

    switch (state) {
    case SyncResult::Success:
    case SyncResult::SyncPrepare:
        // Success should only be shown if all folders were fine
        _fileProviderDomainsWithErrors.erase(account->userIdAtHostWithPort());
        break;
    case SyncResult::Error:
    case SyncResult::SetupError:
    case SyncResult::Problem:
    case SyncResult::Undefined:
        _fileProviderDomainsWithErrors.insert(account->userIdAtHostWithPort());
    case SyncResult::SyncRunning:
    case SyncResult::NotYetStarted:
    case SyncResult::Paused:
    case SyncResult::SyncAbortRequested:
        break;
    }
        
    setSyncState(state);
}
#endif

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
    } else if (!hasConfiguredSyncSource(_accountState)) {
        setSyncStatusString(tr("Sync paused"));
        setSyncIcon(Theme::instance()->pause());
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
        if (!folder || folder->accountState() != _accountState.data()) {
            continue;
        }

        onFolderSyncStateChanged(folder);
        syncStateFallbackNeeded = false;
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    if (_accountState) {
        const auto account = _accountState->account();
        const auto userIdAtHostWithPort = account->userIdAtHostWithPort();

        if (Mac::FileProviderSettingsController::instance()->vfsEnabledForAccount(userIdAtHostWithPort)) {
            const auto lastKnownSyncState = Mac::FileProvider::instance()->service()->latestReceivedSyncStatusForAccount(account);
            onFileProviderDomainSyncStateChanged(account, lastKnownSyncState);
            syncStateFallbackNeeded = false;
        }
    }
#endif

    if (syncStateFallbackNeeded) {
        setSyncStateToConnectedState();
    }
}
}
