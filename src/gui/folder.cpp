/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/syncjournaldb.h"
#include "config.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "folder.h"
#include "folderman.h"
#include "logger.h"
#include "configfile.h"
#include "networkjobs.h"
#include "common/syncjournalfilerecord.h"
#include "syncresult.h"
#include "clientproxy.h"
#include "syncengine.h"
#include "syncrunfilelog.h"
#include "socketapi/socketapi.h"
#include "theme.h"
#include "filesystem.h"
#include "localdiscoverytracker.h"
#include "csync_exclude.h"
#include "common/vfs.h"
#include "creds/abstractcredentials.h"
#include "settingsdialog.h"
#include "vfsdownloaderrordialog.h"

#ifdef Q_OS_MACOS
#include "common/utility_mac_sandbox.h"
#endif

#include <QTimer>
#include <QUrl>
#include <QDir>
#include <QSettings>

#include <QMessageBox>
#include <QPushButton>
#include <QApplication>
#include <type_traits>

namespace {
#ifndef VERSION_C
#define VERSION_C
constexpr auto versionC = "version";
#endif
}

namespace OCC {

Q_LOGGING_CATEGORY(lcFolder, "nextcloud.gui.folder", QtInfoMsg)

Folder::Folder(const FolderDefinition &definition,
    AccountState *accountState, std::unique_ptr<Vfs> vfs,
    QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _definition(definition)
    , _lastSyncDuration(0)
    , _journal(_definition.absoluteJournalPath())
    , _fileLog(new SyncRunFileLog)
    , _vfs(vfs.release())
{
    _timeSinceLastSyncStart.start();
    _timeSinceLastSyncDone.start();

    SyncResult::Status status = SyncResult::NotYetStarted;
    if (definition.paused) {
        status = SyncResult::Paused;
    }
    _syncResult.setStatus(status);

    // check if the local path exists
    const auto folderOk = checkLocalPath();

    _syncResult.setFolder(_definition.alias);

    _engine.reset(new SyncEngine(_accountState->account(), path(), initializeSyncOptions(), remotePath(), &_journal));
    // pass the setting if hidden files are to be ignored, will be read in csync_update
    _engine->setIgnoreHiddenFiles(_definition.ignoreHiddenFiles);

    ConfigFile::setupDefaultExcludeFilePaths(_engine->excludedFiles());
    if (!reloadExcludes())
        qCWarning(lcFolder, "Could not read system exclude file");

    connect(_accountState.data(), &AccountState::termsOfServiceChanged,
            this, [this] ()
            {
                setSyncPaused(_accountState->state() == AccountState::NeedToSignTermsOfService);
            });

    connect(_accountState.data(), &AccountState::isConnectedChanged, this, &Folder::canSyncChanged);
    connect(_engine.data(), &SyncEngine::rootEtag, this, &Folder::etagRetrievedFromSyncEngine);
    connect(_engine.data(), &SyncEngine::rootFileIdReceived, this, &Folder::rootFileIdReceivedFromSyncEngine);

    connect(_engine.data(), &SyncEngine::started, this, &Folder::slotSyncStarted, Qt::QueuedConnection);
    connect(_engine.data(), &SyncEngine::finished, this, &Folder::slotSyncFinished, Qt::QueuedConnection);

    connect(_engine.data(), &SyncEngine::aboutToRemoveAllFiles,
        this, &Folder::slotAboutToRemoveAllFiles);
    connect(_engine.data(), &SyncEngine::transmissionProgress, this, &Folder::slotTransmissionProgress);
    connect(_engine.data(), &SyncEngine::itemCompleted,
        this, &Folder::slotItemCompleted);
    connect(_engine.data(), &SyncEngine::newBigFolder,
        this, &Folder::slotNewBigFolderDiscovered);
    connect(_engine.data(), &SyncEngine::existingFolderNowBig, this, &Folder::slotExistingFolderNowBig);
    connect(_engine.data(), &SyncEngine::seenLockedFile, FolderMan::instance(), &FolderMan::slotSyncOnceFileUnlocks);
    connect(_engine.data(), &SyncEngine::aboutToPropagate,
        this, &Folder::slotLogPropagationStart);
    connect(_engine.data(), &SyncEngine::syncError, this, &Folder::slotSyncError);

    connect(_engine.data(), &SyncEngine::addErrorToGui, this, &Folder::slotAddErrorToGui);

    _scheduleSelfTimer.setSingleShot(true);
    _scheduleSelfTimer.setInterval(SyncEngine::minimumFileAgeForUpload);
    connect(&_scheduleSelfTimer, &QTimer::timeout,
        this, &Folder::slotScheduleThisFolder);

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::folderConflicts,
        this, &Folder::slotFolderConflicts);

    _localDiscoveryTracker.reset(new LocalDiscoveryTracker);
    connect(_engine.data(), &SyncEngine::finished,
        _localDiscoveryTracker.data(), &LocalDiscoveryTracker::slotSyncFinished);
    connect(_engine.data(), &SyncEngine::itemCompleted,
        _localDiscoveryTracker.data(), &LocalDiscoveryTracker::slotItemCompleted);

    connect(_accountState->account().data(), &Account::capabilitiesChanged, this, &Folder::slotCapabilitiesChanged);

    connect(_accountState->account().data(), &Account::wantsFoldersSynced, this, [this] () {
        _engine->setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::FilesystemOnly);
        QMetaObject::invokeMethod(_engine.data(), "startSync", Qt::QueuedConnection);
    });

    // Potentially upgrade suffix vfs to windows vfs
    Q_ASSERT(_vfs);
    if (_definition.virtualFilesMode == Vfs::WithSuffix
        && _definition.upgradeVfsMode) {
        if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            if (auto winvfs = createVfsFromPlugin(Vfs::WindowsCfApi)) {
                // Wipe the existing suffix files from fs and journal
                SyncEngine::wipeVirtualFiles(path(), _journal, *_vfs);

                // Then switch to winvfs mode
                _vfs.reset(winvfs.release());
                _definition.virtualFilesMode = Vfs::WindowsCfApi;
            }
        }
        saveToSettings();
    }

    if (folderOk) {
        // Initialize the vfs plugin
        startVfs();
    }
}

Folder::~Folder()
{
    // If wipeForRemoval() was called the vfs has already shut down.
    if (_vfs)
        _vfs->stop();

    // Reset then engine first as it will abort and try to access members of the Folder
    _engine.reset();

    // macOS sandbox: _securityScopedAccess is declared as the first member,
    // so it is destroyed last by C++ reverse-declaration-order destruction.
    // This ensures sandbox access outlives all filesystem-accessing members.
}

#ifdef Q_OS_MACOS
void Folder::setSecurityScopedAccess(std::unique_ptr<Utility::MacSandboxPersistentAccess> access)
{
    _securityScopedAccess = std::move(access);
}
#endif

bool Folder::checkLocalPath()
{
    const QFileInfo fi(_definition.localPath);
    _canonicalLocalPath = fi.canonicalFilePath();
#ifdef Q_OS_MACOS
    // Workaround QTBUG-55896  (Should be fixed in Qt 5.8)
    _canonicalLocalPath = _canonicalLocalPath.normalized(QString::NormalizationForm_C);
#endif
    if (_canonicalLocalPath.isEmpty()) {
        qCWarning(lcFolder) << "Broken symlink:" << _definition.localPath;
        _canonicalLocalPath = _definition.localPath;
    }

    _canonicalLocalPath = Utility::trailingSlashPath(_canonicalLocalPath);

    if (FileSystem::isDir(_definition.localPath) && FileSystem::isReadable(_definition.localPath)) {
        qCDebug(lcFolder) << "Checked local path ok";
    } else {
        QString error;
        // Check directory again
        if (!FileSystem::fileExists(_definition.localPath, fi)) {
            error = tr("Please choose a different location. The folder %1 doesn't exist.").arg(_definition.localPath);
        } else if (!fi.isDir()) {
            error = tr("Please choose a different location. %1 isn't a valid folder.").arg(_definition.localPath);
        } else if (!fi.isReadable()) {
            error = tr("Please choose a different location. %1 isn't a readable folder.").arg(_definition.localPath);
        }
        if (!error.isEmpty()) {
            _syncResult.appendErrorString(error);
            _syncResult.setStatus(SyncResult::SetupError);
            return false;
        }
    }
    return true;
}

QString Folder::shortGuiRemotePathOrAppName() const
{
    if (remotePath().length() > 0 && remotePath() != QLatin1String("/")) {
        QString a = QFile(remotePath()).fileName();
        if (a.startsWith('/')) {
            a = a.remove(0, 1);
        }
        return a;
    } else {
        return Theme::instance()->appNameGUI();
    }
}

QString Folder::sidebarDisplayName() const
{
    auto displayName = shortGuiRemotePathOrAppName();
    if (AccountManager::instance()->accounts().size() > 1) {
        displayName = QStringLiteral("%1 - %2").arg(displayName, accountState()->account()->shortcutName());
    }

    return displayName;
}

QString Folder::alias() const
{
    return _definition.alias;
}

QString Folder::path() const
{
    return _canonicalLocalPath;
}

QString Folder::shortGuiLocalPath() const
{
    QString p = _definition.localPath;
    const auto home = Utility::trailingSlashPath(QDir::homePath());

    if (p.startsWith(home)) {
        p = p.mid(home.length());
    }
    if (p.length() > 1 && p.endsWith('/')) {
        p.chop(1);
    }
    return QDir::toNativeSeparators(p);
}

bool Folder::ignoreHiddenFiles()
{
    bool re(_definition.ignoreHiddenFiles);
    return re;
}

void Folder::setIgnoreHiddenFiles(bool ignore)
{
    _definition.ignoreHiddenFiles = ignore;
}

QString Folder::cleanPath() const
{
    QString cleanedPath = QDir::cleanPath(_canonicalLocalPath);

    if (cleanedPath.length() == 3 && cleanedPath.endsWith(":/"))
        cleanedPath.remove(2, 1);

    return cleanedPath;
}

bool Folder::isBusy() const
{
    return isSyncRunning();
}

bool Folder::isSyncRunning() const
{
    return _engine->isSyncRunning() || (_vfs && _vfs->isHydrating());
}

QString Folder::remotePath() const
{
    return _definition.targetPath;
}

QString Folder::remotePathTrailingSlash() const
{
    return Utility::trailingSlashPath(remotePath());
}

QString Folder::fulllRemotePathToPathInSyncJournalDb(const QString &fullRemotePath) const
{
    return Utility::fullRemotePathToRemoteSyncRootRelative(fullRemotePath, remotePathTrailingSlash());
}

QUrl Folder::remoteUrl() const
{
    return Utility::concatUrlPath(_accountState->account()->davUrl(), remotePath());
}

bool Folder::syncPaused() const
{
    return _definition.paused;
}

bool Folder::canSync() const
{
    return !syncPaused() && accountState()->isConnected() && _syncResult.status() != SyncResult::SetupError;
}

void Folder::setSyncPaused(bool paused)
{
    if (paused == _definition.paused) {
        return;
    }

    _definition.paused = paused;
    saveToSettings();

    if (!paused) {
        setSyncState(SyncResult::NotYetStarted);
    } else {
        setSyncState(SyncResult::Paused);
    }
    emit syncPausedChanged(this, paused);
    emit syncStateChange();
    emit canSyncChanged();
}

void Folder::setSyncState(SyncResult::Status state)
{
    if (_silenceErrorsUntilNextSync && state == SyncResult::Error) {
        _syncResult.setStatus(SyncResult::Status::Success);
        return;
    }
    _syncResult.setStatus(state);
}

SyncResult Folder::syncResult() const
{
    return _syncResult;
}

void Folder::prepareToSync()
{
    _syncResult.reset();
    _syncResult.setStatus(SyncResult::NotYetStarted);
}

void Folder::slotRunEtagJob()
{
    qCInfo(lcFolder) << "Trying to check" << remoteUrl().toString() << "for changes via ETag check. (time since last sync:" << (_timeSinceLastSyncDone.elapsed() / 1000) << "s)";

    AccountPtr account = _accountState->account();

    if (_requestEtagJob) {
        qCInfo(lcFolder) << remoteUrl().toString() << "has ETag job queued, not trying to sync";
        return;
    }

    if (!canSync()) {
        qCInfo(lcFolder) << "Not syncing.  :" << remoteUrl().toString() << _definition.paused << AccountState::stateString(_accountState->state());
        return;
    }

    // Do the ordinary etag check for the root folder and schedule a
    // sync if it's different.

    _requestEtagJob = new RequestEtagJob(account, remotePath(), this);
    _requestEtagJob->setTimeout(60 * 1000);
    // check if the etag is different when retrieved
    QObject::connect(_requestEtagJob.data(), &RequestEtagJob::etagRetrieved, this, &Folder::etagRetrieved);
    FolderMan::instance()->slotScheduleETagJob(alias(), _requestEtagJob);
    // The _requestEtagJob is auto deleting itself on finish. Our guard pointer _requestEtagJob will then be null.
}

void Folder::etagRetrieved(const QByteArray &etag, const QDateTime &tp)
{
    // re-enable sync if it was disabled because network was down
    FolderMan::instance()->setSyncEnabled(true);

    if (_lastEtag != etag) {
        qCInfo(lcFolder) << "Compare etag with previous etag: last:" << _lastEtag << ", received:" << etag << "-> CHANGED";
        _lastEtag = etag;
        slotScheduleThisFolder();
    }

    _accountState->tagLastSuccessfullETagRequest(tp);
}

void Folder::etagRetrievedFromSyncEngine(const QByteArray &etag, const QDateTime &time)
{
    qCInfo(lcFolder) << "Root etag from during sync:" << etag;
    accountState()->tagLastSuccessfullETagRequest(time);
    _lastEtag = etag;
}

void Folder::rootFileIdReceivedFromSyncEngine(const qint64 fileId)
{
    qCDebug(lcFolder).nospace() << "retrieved root fileId=" << fileId;
    _rootFileId = fileId;
}

void Folder::showSyncResultPopup()
{
    if (_syncResult.firstItemNew()) {
        createGuiLog(_syncResult.firstItemNew()->destination(), LogStatusNew, _syncResult.numNewItems());
    }
    if (_syncResult.firstItemDeleted()) {
        createGuiLog(_syncResult.firstItemDeleted()->destination(), LogStatusRemove, _syncResult.numRemovedItems());
    }
    if (_syncResult.firstItemUpdated()) {
        createGuiLog(_syncResult.firstItemUpdated()->destination(), LogStatusUpdated, _syncResult.numUpdatedItems());
    }

    if (_syncResult.firstItemRenamed()) {
        LogStatus status(LogStatusRename);
        // if the path changes it's rather a move
        QDir renTarget = QFileInfo(_syncResult.firstItemRenamed()->_renameTarget).dir();
        QDir renSource = QFileInfo(_syncResult.firstItemRenamed()->_file).dir();
        if (renTarget != renSource) {
            status = LogStatusMove;
        }
        createGuiLog(_syncResult.firstItemRenamed()->_file, status,
            _syncResult.numRenamedItems(), _syncResult.firstItemRenamed()->_renameTarget);
    }

    if (_syncResult.firstNewConflictItem()) {
        createGuiLog(_syncResult.firstNewConflictItem()->destination(), LogStatusConflict, _syncResult.numNewConflictItems());
    }
    if (int errorCount = _syncResult.numErrorItems()) {
        createGuiLog(_syncResult.firstItemError()->_file, LogStatusError, errorCount);
    }

    if (int lockedCount = _syncResult.numLockedItems()) {
        createGuiLog(_syncResult.firstItemLocked()->_file, LogStatusFileLocked, lockedCount);
    }

    qCInfo(lcFolder) << "Folder" << _syncResult.folder() << "sync result: " << _syncResult.status();
}

void Folder::createGuiLog(const QString &filename, LogStatus status, int count,
    const QString &renameTarget)
{
    if (count > 0) {
        Logger *logger = Logger::instance();

        QString file = QDir::toNativeSeparators(filename);
        QString text;

        switch (status) {
        case LogStatusRemove:
            if (count > 1) {
                text = tr("%1 and %n other file(s) have been removed.", "", count - 1).arg(file);
            } else {
                text = tr("%1 has been removed.", "%1 names a file.").arg(file);
            }
            break;
        case LogStatusNew:
            if (count > 1) {
                text = tr("%1 and %n other file(s) have been added.", "", count - 1).arg(file);
            } else {
                text = tr("%1 has been added.", "%1 names a file.").arg(file);
            }
            break;
        case LogStatusUpdated:
            if (count > 1) {
                text = tr("%1 and %n other file(s) have been updated.", "", count - 1).arg(file);
            } else {
                text = tr("%1 has been updated.", "%1 names a file.").arg(file);
            }
            break;
        case LogStatusRename:
            if (count > 1) {
                text = tr("%1 has been renamed to %2 and %n other file(s) have been renamed.", "", count - 1).arg(file, renameTarget);
            } else {
                text = tr("%1 has been renamed to %2.", "%1 and %2 name files.").arg(file, renameTarget);
            }
            break;
        case LogStatusMove:
            if (count > 1) {
                text = tr("%1 has been moved to %2 and %n other file(s) have been moved.", "", count - 1).arg(file, renameTarget);
            } else {
                text = tr("%1 has been moved to %2.").arg(file, renameTarget);
            }
            break;
        case LogStatusConflict:
            if (count > 1) {
                text = tr("%1 has and %n other file(s) have sync conflicts.", "", count - 1).arg(file);
            } else {
                text = tr("%1 has a sync conflict. Please check the conflict file!").arg(file);
            }
            break;
        case LogStatusError:
            if (count > 1) {
                text = tr("%1 and %n other file(s) could not be synced due to errors. See the log for details.", "", count - 1).arg(file);
            } else {
                text = tr("%1 could not be synced due to an error. See the log for details.").arg(file);
            }
            break;
        case LogStatusFileLocked:
            if (count > 1) {
                text = tr("%1 and %n other file(s) are currently locked.", "", count -1).arg(file);
            } else {
                text = tr("%1 is currently locked.").arg(file);
            }
            break;
        }

        if (!text.isEmpty()) {
            // Ignores the settings in case of an error or conflict
            if(status == LogStatusError || status == LogStatusConflict)
                logger->postGuiLog(tr("Sync Activity"), text);
        }
    }
}

void Folder::startVfs()
{
    Q_ASSERT(_vfs);
    Q_ASSERT(_vfs->mode() == _definition.virtualFilesMode);

    const auto result = Vfs::checkAvailability(path(), _vfs->mode());
    if (!result) {
        _syncResult.appendErrorString(result.error());
        _syncResult.setStatus(SyncResult::SetupError);
        return;
    }

    const auto displayName = sidebarDisplayName();
    qCDebug(lcFolder) << "Display name for VFS folder will be:" << displayName;
    VfsSetupParams vfsParams;
    vfsParams.filesystemPath = path();
    vfsParams.displayName = displayName;
    vfsParams.alias = alias();
    vfsParams.navigationPaneClsid = navigationPaneClsid().toString();
    vfsParams.remotePath = remotePathTrailingSlash();
    vfsParams.account = _accountState->account();
    vfsParams.journal = &_journal;
    vfsParams.providerName = Theme::instance()->appNameGUI();
    vfsParams.providerVersion = Theme::instance()->version();
    vfsParams.multipleAccountsRegistered = AccountManager::instance()->accounts().size() > 1;

    connect(_vfs.data(), &Vfs::beginHydrating, this, &Folder::slotHydrationStarts);
    connect(_vfs.data(), &Vfs::doneHydrating, this, &Folder::slotHydrationDone);
    connect(_vfs.data(), &Vfs::failureHydrating, this, &Folder::slotHydrationFailed);

    connect(&_engine->syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
            _vfs.data(), &Vfs::fileStatusChanged);

    _vfs->start(vfsParams);

    // Immediately mark the sqlite temporaries as excluded. They get recreated
    // on db-open and need to get marked again every time.
    const auto stateDbFile = _journal.databaseFilePath();
    const auto stateDbWalFile = QString(stateDbFile + QStringLiteral("-wal"));
    const auto stateDbShmFile = QString(stateDbFile + QStringLiteral("-shm"));

    FileSystem::setFileReadOnly(stateDbFile, false);
    FileSystem::setFileReadOnly(stateDbWalFile, false);
    FileSystem::setFileReadOnly(stateDbShmFile, false);
    FileSystem::setFolderPermissions(path(), FileSystem::FolderPermissions::ReadWrite);

    _journal.open();
    _vfs->fileStatusChanged(stateDbFile + "-wal", SyncFileStatus::StatusExcluded);
    _vfs->fileStatusChanged(stateDbFile + "-shm", SyncFileStatus::StatusExcluded);
}

int Folder::slotDiscardDownloadProgress()
{
    // Delete from journal and from filesystem.
    QDir folderpath(_definition.localPath);
    QSet<QString> keep_nothing;
    const QVector<SyncJournalDb::DownloadInfo> deleted_infos =
        _journal.getAndDeleteStaleDownloadInfos(keep_nothing);
    for (const auto &deleted_info : deleted_infos) {
        const QString tmppath = folderpath.filePath(deleted_info._tmpfile);
        qCInfo(lcFolder) << "Deleting temporary file: " << tmppath;
        FileSystem::remove(tmppath);
    }
    return deleted_infos.size();
}

int Folder::downloadInfoCount()
{
    return _journal.downloadInfoCount();
}

int Folder::errorBlackListEntryCount()
{
    return _journal.errorBlackListEntryCount();
}

int Folder::slotWipeErrorBlacklist()
{
    return _journal.wipeErrorBlacklist();
}

void Folder::slotWatchedPathChanged(const QStringView &path, const ChangeReason reason)
{
    if (!path.startsWith(this->path())) {
        qCDebug(lcFolder) << "Changed path is not contained in folder, ignoring:" << path;
        return;
    }

    auto relativePath = path.mid(this->path().size());

    if (_vfs) {
        if (pathIsIgnored(path.toString())) {
            const auto pinState = _vfs->pinState(relativePath.toString());
            if ((!pinState || *pinState != PinState::Excluded) && FileSystem::fileExists(relativePath.toString())) {
                if (!_vfs->setPinState(relativePath.toString(), PinState::Excluded)) {
                    qCWarning(lcFolder) << "Could not set pin state of" << relativePath << "to excluded";
                }
            }
            return;
        } else {
            const auto pinState = _vfs->pinState(relativePath.toString());
            if (pinState && *pinState == PinState::Excluded) {
                if (!_vfs->setPinState(relativePath.toString(), PinState::Inherited)) {
                    qCWarning(lcFolder) << "Could not switch pin state of" << relativePath << "from" << *pinState << "to inherited";
                }
            }
        }
    }

    // Add to list of locally modified paths
    //
    // We do this before checking for our own sync-related changes to make
    // extra sure to not miss relevant changes.
    auto relativePathBytes = relativePath.toUtf8();
    _localDiscoveryTracker->addTouchedPath(relativePathBytes);

// The folder watcher fires a lot of bogus notifications during
// a sync operation, both for actual user files and the database
// and log. Therefore we check notifications against operations
// the sync is doing to filter out our own changes.
#ifdef Q_OS_MACOS
// On OSX the folder watcher does not report changes done by our
// own process. Therefore nothing needs to be done here!
#else
    // Use the path to figure out whether it was our own change
    if (_engine->wasFileTouched(path.toString())) {
        qCDebug(lcFolder) << "Changed path was touched by SyncEngine, ignoring:" << path;
        return;
    }
    qCDebug(lcFolder) << "Detected changes in paths:" << path;
#endif

    SyncJournalFileRecord record;
    if (!_journal.getFileRecord(relativePathBytes, &record)) {
        qCWarning(lcFolder) << "could not get file from local DB" << relativePathBytes;
    }
    if (reason != ChangeReason::UnLock) {
        // Check that the mtime/size actually changed or there was
        // an attribute change (pin state) that caused the notification
        bool spurious = false;
        if (record.isValid()
            && !FileSystem::fileChanged(path.toString(), record._fileSize, record._modtime) && _vfs) {
            spurious = true;
            if (auto pinState = _vfs->pinState(relativePath.toString())) {
                qCDebug(lcFolder) << "PinState for" << relativePath << "is" << *pinState;
                if (*pinState == PinState::Unspecified) {
                    spurious = false;
                }
                if (*pinState == PinState::AlwaysLocal && record.isVirtualFile()) {
                    spurious = false;
                }
                if (*pinState == PinState::OnlineOnly && record.isFile()) {
                    spurious = false;
                }
            } else {
                spurious = false;
            }
            if (spurious && !_vfs->isPlaceHolderInSync(path.toString())) {
                spurious = false;
            }
        }
        if (spurious) {
            qCDebug(lcFolder) << "Ignoring spurious notification for file" << relativePath;
            return; // probably a spurious notification
        }
    }
    warnOnNewExcludedItem(record, relativePath);

    emit watchedFileChangedExternally(path.toString());

    // Also schedule this folder for a sync, but only after some delay:
    // The sync will not upload files that were changed too recently.
    scheduleThisFolderSoon();
}

void Folder::slotFilesLockReleased(const QSet<QString> &files)
{
    qCDebug(lcFolder) << "Going to unlock office files" << files;

    for (const auto &file : files) {
        const auto fileRecordPath = fileFromLocalPath(file);
        SyncJournalFileRecord rec;
        const auto isFileRecordValid = journalDb()->getFileRecord(fileRecordPath, &rec) && rec.isValid();
        if (isFileRecordValid) {
            const auto itemPointer = SyncFileItem::fromSyncJournalFileRecord(rec);
            [[maybe_unused]] const auto result = _vfs->updatePlaceholderMarkInSync(path() + rec.path(), *itemPointer);
        }
        const auto canUnlockFile = isFileRecordValid
            && rec._lockstate._locked
            && (!_accountState->account()->capabilities().filesLockTypeAvailable() || rec._lockstate._lockOwnerType == static_cast<qint64>(SyncFileItem::LockOwnerType::TokenLock))
            && rec._lockstate._lockOwnerId == _accountState->account()->davUser();

        if (!canUnlockFile) {
            qCInfo(lcFolder) << "Skipping file" << file
                             << "with rec.isValid():" << rec.isValid()
                             << "and rec._lockstate._lockOwnerId:" << rec._lockstate._lockOwnerId
                             << "and lock type:" << rec._lockstate._lockOwnerType
                             << "and davUser:" << _accountState->account()->davUser();
            continue;
        }
        const QString remoteFilePath = remotePathTrailingSlash() + rec.path();
        qCDebug(lcFolder) << "Unlocking an office file" << remoteFilePath;
        _officeFileLockReleaseUnlockSuccess = connect(_accountState->account().data(), &Account::lockFileSuccess, this, [this, remoteFilePath]() {
            disconnect(_officeFileLockReleaseUnlockSuccess);
            qCDebug(lcFolder) << "Unlocking an office file succeeded" << remoteFilePath;
            startSync();
        });
        _officeFileLockReleaseUnlockFailure = connect(_accountState->account().data(), &Account::lockFileError, this, [this, remoteFilePath](const QString &message) {
            disconnect(_officeFileLockReleaseUnlockFailure);
            qCWarning(lcFolder) << "Failed to unlock a file:" << remoteFilePath << message;
        });
        const auto lockOwnerType = static_cast<SyncFileItem::LockOwnerType>(rec._lockstate._lockOwnerType);
        _accountState->account()->setLockFileState(remoteFilePath,
                                                   remotePathTrailingSlash(),
                                                   path(),
                                                   rec._etag,
                                                   journalDb(),
                                                   SyncFileItem::LockStatus::UnlockedItem,
                                                   lockOwnerType);
    }
}

void Folder::slotFilesLockImposed(const QSet<QString> &files)
{
    qCDebug(lcFolder) << "Lock files detected for office files" << files;
    for (const auto &file : files) {
        const auto fileRecordPath = fileFromLocalPath(file);
        SyncJournalFileRecord rec;
        if (journalDb()->getFileRecord(fileRecordPath, &rec) && rec.isValid()) {
            const auto itemPointer = SyncFileItem::fromSyncJournalFileRecord(rec);
            [[maybe_unused]] const auto result = _vfs->updatePlaceholderMarkInSync(path() + rec.path(), *itemPointer);
        }
    }
}

void Folder::slotLockedFilesFound(const QSet<QString> &files)
{
    qCDebug(lcFolder) << "Found new lock files" << files;

    for (const auto &file : files) {
        const auto fileRecordPath = fileFromLocalPath(file);
        SyncJournalFileRecord rec;

        const auto canLockFile = journalDb()->getFileRecord(fileRecordPath, &rec) && rec.isValid() && !rec._lockstate._locked;

        if (!canLockFile) {
            qCDebug(lcFolder) << "Skipping locking file" << file << "with rec.isValid():" << rec.isValid()
                              << "and rec._lockstate._lockOwnerId:" << rec._lockstate._lockOwnerId << "and davUser:" << _accountState->account()->davUser();
            continue;
        }

        const QString remoteFilePath = remotePathTrailingSlash() + rec.path();
        qCDebug(lcFolder) << "Automatically locking file on server" << remoteFilePath;
        _fileLockSuccess = connect(_accountState->account().data(), &Account::lockFileSuccess, this, [this, remoteFilePath] {
            disconnect(_fileLockSuccess);
            disconnect(_fileLockFailure);
            qCDebug(lcFolder) << "Locking file succeeded" << remoteFilePath;
            startSync();
        });
        _fileLockFailure = connect(_accountState->account().data(), &Account::lockFileError, this, [this, remoteFilePath](const QString &message) {
            disconnect(_fileLockSuccess);
            disconnect(_fileLockFailure);
            qCWarning(lcFolder) << "Failed to lock a file:" << remoteFilePath << message;
        });
        _accountState->account()->setLockFileState(remoteFilePath,
                                                   remotePathTrailingSlash(),
                                                   path(),
                                                   rec._etag,
                                                   journalDb(),
                                                   SyncFileItem::LockStatus::LockedItem,
                                                   SyncFileItem::LockOwnerType::TokenLock);
    }
}

void Folder::implicitlyHydrateFile(const QString &relativepath)
{
    qCInfo(lcFolder) << "Implicitly hydrate virtual file:" << relativepath;

    // Set in the database that we should download the file
    SyncJournalFileRecord record;
    ;
    if (!_journal.getFileRecord(relativepath.toUtf8(), &record)) {
        qCWarning(lcFolder) << "could not get file from local DB" << relativepath;
        return;
    }
    if (!record.isValid()) {
        qCInfo(lcFolder) << "Did not find file in db";
        return;
    }
    if (!record.isVirtualFile()) {
        qCInfo(lcFolder) << "The file is not virtual";
        return;
    }

    record._type = ItemTypeVirtualFileDownload;

    const auto result = _journal.setFileRecord(record);
    if (!result) {
        qCWarning(lcFolder) << "Error when setting the file record to the database" << record._path << result.error();
        return;
    }

    // Change the file's pin state if it's contradictory to being hydrated
    // (suffix-virtual file's pin state is stored at the hydrated path)
    const auto pin = _vfs->pinState(relativepath);
    if (pin && *pin == PinState::OnlineOnly) {
        if (!_vfs->setPinState(relativepath, PinState::Unspecified)) {
            qCWarning(lcFolder) << "Could not set pin state of" << relativepath << "to unspecified";
        }
    }

    // Add to local discovery
    schedulePathForLocalDiscovery(relativepath);
    slotScheduleThisFolder();
}

void Folder::setVirtualFilesEnabled(bool enabled)
{
    auto newMode = _definition.virtualFilesMode;
    if (!enabled) {
        newMode = Vfs::Off;
    } else if (newMode == Vfs::Off) {
        newMode = bestAvailableVfsMode();
    }

    if (newMode != _definition.virtualFilesMode) {
        // TODO: Must wait for current sync to finish!
        SyncEngine::wipeVirtualFiles(path(), _journal, *_vfs);

        _vfs->stop();
        _vfs->unregisterFolder();

        disconnect(_vfs.data(), nullptr, this, nullptr);
        disconnect(&_engine->syncFileStatusTracker(), nullptr, _vfs.data(), nullptr);

        _vfs.reset(createVfsFromPlugin(newMode).release());

        _definition.virtualFilesMode = newMode;
        startVfs();
        _saveInFoldersWithPlaceholders = newMode != Vfs::Off;
        if (newMode != Vfs::Off) {
            switchToVirtualFiles();
        }
        saveToSettings();
    }
}

void Folder::setRootPinState(PinState state)
{
    if (!_vfs->setPinState(QString(), state)) {
        qCWarning(lcFolder) << "Could not set root pin state of" << _definition.alias;
    }

    // We don't actually need discovery, but it's important to recurse
    // into all folders, so the changes can be applied.
    slotNextSyncFullLocalDiscovery();
}

void Folder::switchToVirtualFiles()
{
    SyncEngine::switchToVirtualFiles(path(), _journal, *_vfs);
    _hasSwitchedToVfs = true;
}

void Folder::processSwitchedToVirtualFiles()
{
    if (_hasSwitchedToVfs) {
        _hasSwitchedToVfs = false;
        saveToSettings();
    }
}

bool Folder::supportsSelectiveSync() const
{
    return !virtualFilesEnabled() && !isVfsOnOffSwitchPending();
}

void Folder::saveToSettings() const
{
    // Remove first to make sure we don't get duplicates
    removeFromSettings();

    auto settings = _accountState->settings();
    QString settingsGroup = QStringLiteral("Multifolders");

    // True if the folder path appears in only one account
    const auto folderMap = FolderMan::instance()->map();
    const auto oneAccountOnly = std::none_of(folderMap.cbegin(), folderMap.cend(), [this](const auto *other) {
        return other != this && other->cleanPath() == this->cleanPath();
    });

    if (virtualFilesEnabled() || _saveInFoldersWithPlaceholders) {
        // If virtual files are enabled or even were enabled at some point,
        // save the folder to a group that will not be read by older (<2.5.0) clients.
        // The name is from when virtual files were called placeholders.
        settingsGroup = QStringLiteral("FoldersWithPlaceholders");
    } else if (_saveBackwardsCompatible || oneAccountOnly) {
        // The folder is saved to backwards-compatible "Folders"
        // section only if it has the migrate flag set (i.e. was in
        // there before) or if the folder is the only one for the
        // given target path.
        // This ensures that older clients will not read a configuration
        // where two folders for different accounts point at the same
        // local folders.
        settingsGroup = QStringLiteral("Folders");
    }

    settings->beginGroup(settingsGroup);
    // Note: Each of these groups might have a "version" tag, but that's
    //       currently unused.
    settings->beginGroup(FolderMan::escapeAlias(_definition.alias));
    FolderDefinition::save(*settings, _definition);

    settings->sync();
    qCInfo(lcFolder) << "Saved folder" << _definition.alias << "to settings, status" << settings->status();
}

void Folder::removeFromSettings() const
{
    auto settings = _accountState->settings();
    settings->beginGroup(QLatin1String("Folders"));
    settings->remove(FolderMan::escapeAlias(_definition.alias));
    settings->endGroup();
    settings->beginGroup(QLatin1String("Multifolders"));
    settings->remove(FolderMan::escapeAlias(_definition.alias));
    settings->endGroup();
    settings->beginGroup(QLatin1String("FoldersWithPlaceholders"));
    settings->remove(FolderMan::escapeAlias(_definition.alias));
}

bool Folder::pathIsIgnored(const QString &path) const
{
    if (path.isEmpty()) {
        return true;
    }

#ifndef OWNCLOUD_TEST
    if (isFileExcludedAbsolute(path) && !Utility::isConflictFile(path)) {
        qCDebug(lcFolder) << "* Ignoring file" << path;
        return true;
    }
#endif
    return false;
}

void Folder::appendPathToSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType listType)
{
    const auto folderPath = Utility::trailingSlashPath(path);
    const auto journal = journalDb();
    auto ok = false;
    auto list = journal->getSelectiveSyncList(listType, &ok);

    if (ok) {
        list.append(folderPath);
        journal->setSelectiveSyncList(listType, list);
    }
}

void Folder::removePathFromSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType listType)
{
    const auto folderPath = Utility::trailingSlashPath(path);
    const auto journal = journalDb();
    auto ok = false;
    auto list = journal->getSelectiveSyncList(listType, &ok);

    if (ok) {
        list.removeAll(folderPath);
        journal->setSelectiveSyncList(listType, list);
    }
}

void Folder::whitelistPath(const QString &path)
{
    removePathFromSelectiveSyncList(path, SyncJournalDb::SelectiveSyncUndecidedList);
    removePathFromSelectiveSyncList(path, SyncJournalDb::SelectiveSyncBlackList);
    appendPathToSelectiveSyncList(path, SyncJournalDb::SelectiveSyncWhiteList);
}

void Folder::blacklistPath(const QString &path)
{
    removePathFromSelectiveSyncList(path, SyncJournalDb::SelectiveSyncUndecidedList);
    removePathFromSelectiveSyncList(path, SyncJournalDb::SelectiveSyncWhiteList);
    appendPathToSelectiveSyncList(path, SyncJournalDb::SelectiveSyncBlackList);
}

void Folder::migrateBlackListPath(const QString &legacyPath)
{
    if (legacyPath.startsWith(QLatin1Char('/'))) {
        removePathFromSelectiveSyncList(legacyPath, SyncJournalDb::SelectiveSyncBlackList);
        blacklistPath(legacyPath.mid(1));
    }
}

bool Folder::hasFileIds(const QList<qint64>& fileIds) const
{
    return fileIds.contains(_rootFileId) || journalDb()->hasFileIds(fileIds);
}

QString Folder::filePath(const QString& fileName)
{
    const auto folderDir = QDir(_canonicalLocalPath);

#ifdef Q_OS_WIN
    // Edge case time!
    // QDir::filePath checks whether the passed `fileName` is absolute (essentialy by using `!QFileInfo::isRelative()`).
    // In the case it's absolute, the `fileName` will be returned instead of the complete file path.
    //
    // On Windows, if `fileName` starts with a letter followed by a colon (e.g. "A:BCDEF"), it is considered to be an
    // absolute path.
    // Since this method should return the file name file path starting with the canonicalLocalPath, catch that special case here and prefix it ourselves...
    return fileName.length() >= 2 && fileName[1] == ':'
           ? _canonicalLocalPath + fileName
           : folderDir.filePath(fileName);
#else
    return folderDir.filePath(fileName);
#endif
}

bool Folder::isFileExcludedAbsolute(const QString &fullPath) const
{
    return _engine->excludedFiles().isExcluded(fullPath, path(), _definition.ignoreHiddenFiles);
}

bool Folder::isFileExcludedRelative(const QString &relativePath) const
{
    return _engine->excludedFiles().isExcluded(path() + relativePath, path(), _definition.ignoreHiddenFiles);
}

void Folder::slotTerminateSync()
{
    qCInfo(lcFolder) << "folder " << alias() << " Terminating!";

    if (_engine->isSyncRunning()) {
        _engine->abort();

        setSyncState(SyncResult::SyncAbortRequested);
    }
}

void Folder::wipeForRemoval()
{
    disconnectFolderWatcher();

    // Delete files that have been partially downloaded.
    slotDiscardDownloadProgress();

    // Unregister the socket API so it does not keep the .sync_journal file open
    FolderMan::instance()->socketApi()->slotUnregisterPath(alias());
    // Close the sync journal.  Do NOT call any methods that fetch data from it
    // after this point, otherwise the journal is re-opened.  On some systems
    // (Windows) this prevents the removal of the db file as it's open again...
    _journal.close();

    if (!QDir(path()).exists()) {
        qCCritical(lcFolder) << "db files are not going to be deleted, sync folder could not be found at" << path();
        return;
    }

    // Remove db and temporaries
    QString stateDbFile = _engine->journal()->databaseFilePath();

    QFile file(stateDbFile);
    if (file.exists()) {
        if (!file.remove()) {
            qCWarning(lcFolder) << "Failed to remove existing csync StateDB " << stateDbFile;
        } else {
            qCInfo(lcFolder) << "wipe: Removed csync StateDB " << stateDbFile;
        }
    } else {
        qCWarning(lcFolder) << "statedb is empty, can not remove.";
    }

    // Also remove other db related files
    QFile::remove(stateDbFile + ".ctmp");
    QFile::remove(stateDbFile + "-shm");
    QFile::remove(stateDbFile + "-wal");
    QFile::remove(stateDbFile + "-journal");

    _vfs->stop();
    _vfs->unregisterFolder();
    _vfs.reset(nullptr); // warning: folder now in an invalid state
}

bool Folder::reloadExcludes()
{
    return _engine->excludedFiles().reloadExcludeFiles();
}

void Folder::startSync(const QStringList &pathList)
{
    Q_UNUSED(pathList);
    setSilenceErrorsUntilNextSync(false);
    const auto singleItemDiscoveryOptions = _engine->singleItemDiscoveryOptions();
    Q_ASSERT(!singleItemDiscoveryOptions.discoveryDirItem || singleItemDiscoveryOptions.discoveryDirItem->isDirectory());
    if (singleItemDiscoveryOptions.discoveryDirItem && !singleItemDiscoveryOptions.discoveryDirItem->isDirectory()) {
        qCCritical(lcFolder) << "startSync only accepts directory SyncFileItem, not a file.";
    }
    if (isBusy()) {
        qCCritical(lcFolder) << "ERROR csync is still running and new sync requested.";
        return;
    }

    _timeSinceLastSyncStart.start();
    _syncResult.setStatus(SyncResult::SyncPrepare);
    emit syncStateChange();

    qCInfo(lcFolder) << "*** Start syncing " << remoteUrl().toString() << " -" << APPLICATION_NAME << "client version"
                     << qPrintable(Theme::instance()->version());

    _fileLog->start(path());

    if (!reloadExcludes()) {
        slotSyncError(tr("Could not read system exclude file"), ErrorCategory::GenericError);
        QMetaObject::invokeMethod(this, "slotSyncFinished", Qt::QueuedConnection, Q_ARG(bool, false));
        return;
    }

    setDirtyNetworkLimits();
    syncEngine().setSyncOptions(initializeSyncOptions());

    static std::chrono::milliseconds fullLocalDiscoveryInterval = []() {
        auto interval = ConfigFile().fullLocalDiscoveryInterval();
        QByteArray env = qgetenv("OWNCLOUD_FULL_LOCAL_DISCOVERY_INTERVAL");
        if (!env.isEmpty()) {
            interval = std::chrono::milliseconds(env.toLongLong());
        }
        return interval;
    }();
    bool hasDoneFullLocalDiscovery = _timeSinceLastFullLocalDiscovery.isValid();
    bool periodicFullLocalDiscoveryNow =
        fullLocalDiscoveryInterval.count() >= 0 // negative means we don't require periodic full runs
        && _timeSinceLastFullLocalDiscovery.hasExpired(fullLocalDiscoveryInterval.count());

    if (singleItemDiscoveryOptions.isValid() && singleItemDiscoveryOptions.discoveryPath != QStringLiteral("/")) {
        qCInfo(lcFolder) << "Going to sync just one file";
        _engine->setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {singleItemDiscoveryOptions.discoveryPath});
        _localDiscoveryTracker->startSyncPartialDiscovery();
    } else if (_folderWatcher && _folderWatcher->isReliable()
        && hasDoneFullLocalDiscovery
        && !periodicFullLocalDiscoveryNow) {
        qCInfo(lcFolder) << "Allowing local discovery to read from the database";
        _engine->setLocalDiscoveryOptions(
            LocalDiscoveryStyle::DatabaseAndFilesystem,
            _localDiscoveryTracker->localDiscoveryPaths());
        _localDiscoveryTracker->startSyncPartialDiscovery();
    } else {
        qCInfo(lcFolder) << "Forbidding local discovery to read from the database";
        _engine->setLocalDiscoveryOptions(LocalDiscoveryStyle::FilesystemOnly);
        _localDiscoveryTracker->startSyncFullDiscovery();
    }

    _engine->setIgnoreHiddenFiles(_definition.ignoreHiddenFiles);
    _engine->setFilesystemPermissionsReliable(_folderWatcher->canSetPermissions());

    correctPlaceholderFiles();

    QMetaObject::invokeMethod(_engine.data(), "startSync", Qt::QueuedConnection);

    emit syncStarted();
}

void Folder::correctPlaceholderFiles()
{
    if (_definition.virtualFilesMode == Vfs::Off) {
        return;
    }
    static const auto placeholdersCorrectedKey = QStringLiteral("placeholders_corrected");
    const auto placeholdersCorrected = _journal.keyValueStoreGetInt(placeholdersCorrectedKey, 0);
    if (!placeholdersCorrected) {
        qCDebug(lcFolder) << "Make sure all virtual files are placeholder files";
        switchToVirtualFiles();
        _journal.keyValueStoreSet(placeholdersCorrectedKey, true);
    }
}

SyncOptions Folder::initializeSyncOptions() const
{
    SyncOptions opt;
    ConfigFile cfgFile;
    const auto account = _accountState->account();

    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    opt._newBigFolderSizeLimit = newFolderLimit.first ? newFolderLimit.second * 1000LL * 1000LL : -1; // convert from MB to B
    opt._confirmExternalStorage = cfgFile.confirmExternalStorage();
    opt._moveFilesToTrash = cfgFile.moveToTrash();
    opt._vfs = _vfs;

    const auto capsMaxConcurrentChunkUploads = account->capabilities().maxConcurrentChunkUploads();
    opt._parallelNetworkJobs = capsMaxConcurrentChunkUploads > 0
        ? capsMaxConcurrentChunkUploads
        : account->isHttp2Supported() ? 20 : 6;

    // Chunk V2: Size of chunks must be between 5MB and 5GB, except for the last chunk which can be smaller
    const auto cfgMinChunkSize = cfgFile.minChunkSize();
    opt.setMinChunkSize(cfgMinChunkSize);

    if (const auto capsMaxChunkSize = account->capabilities().maxChunkSize(); capsMaxChunkSize) {
        opt.setMaxChunkSize(capsMaxChunkSize);
        opt._initialChunkSize = capsMaxChunkSize;
    } else {
        const auto cfgMaxChunkSize = cfgFile.maxChunkSize();
        opt.setMaxChunkSize(cfgMaxChunkSize);
        opt._initialChunkSize = ::qBound(cfgMinChunkSize, cfgFile.chunkSize(), cfgMaxChunkSize);
    }
    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();

    return opt;
}

void Folder::setDirtyNetworkLimits()
{
    const auto account = _accountState->account();

    ConfigFile cfg;

    int downloadLimit = 0;
    const auto useDownLimit = static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(account->downloadLimitSetting());
    if (useDownLimit >= 1) {
        downloadLimit = account->downloadLimit() * 1000;
    } else if (useDownLimit == 0) {
        downloadLimit = 0;
    }

    int uploadLimit = 0;
    const auto useUpLimit = static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(account->uploadLimitSetting());
    if (useUpLimit >= 1) {
        uploadLimit = account->uploadLimit() * 1000;
    } else if (useUpLimit == 0) {
        uploadLimit = 0;
    }

    _engine->setNetworkLimits(uploadLimit, downloadLimit);
}

void Folder::slotSyncError(const QString &message, ErrorCategory category)
{
    if (!_silenceErrorsUntilNextSync) {
        _syncResult.appendErrorString(message);
        emit ProgressDispatcher::instance()->syncError(alias(), message, category);
    }
}

void Folder::slotAddErrorToGui(SyncFileItem::Status status, const QString &errorMessage, const QString &subject, ErrorCategory category)
{
    emit ProgressDispatcher::instance()->addErrorToGui(alias(), status, errorMessage, subject, category);
}

void Folder::slotSyncStarted()
{
    qCInfo(lcFolder) << "#### Propagation start ####################################################";
    _syncResult.setStatus(SyncResult::SyncRunning);
    emit syncStateChange();
}

void Folder::slotSyncFinished(bool success)
{
    qCInfo(lcFolder) << "Client version" << qPrintable(Theme::instance()->version())
                     << " Qt" << qVersion()
                     << " SSL " << QSslSocket::sslLibraryVersionString().toUtf8().data()
        ;

    bool syncError = !_syncResult.errorStrings().isEmpty();
    if (syncError) {
        qCWarning(lcFolder) << "SyncEngine finished with ERROR";
    } else {
        qCInfo(lcFolder) << "SyncEngine finished without problem.";
    }
    _fileLog->finish();
    showSyncResultPopup();

    auto anotherSyncNeeded = _engine->isAnotherSyncNeeded();

    if (syncError) {
        _syncResult.setStatus(SyncResult::Error);
        if (_silenceErrorsUntilNextSync) {
            _syncResult.setStatus(SyncResult::Status::Success);
        }
    } else if (_syncResult.foundFilesNotSynced()) {
        _syncResult.setStatus(SyncResult::Problem);
    } else if (_definition.paused) {
        // Maybe the sync was terminated because the user paused the folder
        _syncResult.setStatus(SyncResult::Paused);
    } else {
        _syncResult.setStatus(SyncResult::Success);
    }

    // Count the number of syncs that have failed in a row.
    if (_syncResult.status() == SyncResult::Success
        || _syncResult.status() == SyncResult::Problem) {
        _consecutiveFailingSyncs = 0;
    } else {
        _consecutiveFailingSyncs++;
        qCInfo(lcFolder) << "the last" << _consecutiveFailingSyncs << "syncs failed";
    }

    if ((_syncResult.status() == SyncResult::Success
            || _syncResult.status() == SyncResult::Problem)
        && success) {
        if (_engine->lastLocalDiscoveryStyle() == LocalDiscoveryStyle::FilesystemOnly) {
            _timeSinceLastFullLocalDiscovery.start();
        }
    }

    emit syncStateChange();

    // The syncFinished result that is to be triggered here makes the folderman
    // clear the current running sync folder marker.
    // Lets wait a bit to do that because, as long as this marker is not cleared,
    // file system change notifications are ignored for that folder. And it takes
    // some time under certain conditions to make the file system notifications
    // all come in.
    QTimer::singleShot(200, this, &Folder::slotEmitFinishedDelayed);

    _lastSyncDuration = std::chrono::milliseconds(_timeSinceLastSyncStart.elapsed());
    _timeSinceLastSyncDone.start();

    // Increment the follow-up sync counter if necessary.
    if (anotherSyncNeeded == ImmediateFollowUp) {
        _consecutiveFollowUpSyncs++;
        qCInfo(lcFolder) << "another sync was requested by the finished sync, this has"
                         << "happened" << _consecutiveFollowUpSyncs << "times";
    } else {
        _consecutiveFollowUpSyncs = 0;
    }

    // Maybe force a follow-up sync to take place, but only a couple of times.
    if (anotherSyncNeeded == ImmediateFollowUp && _consecutiveFollowUpSyncs <= 3) {
        // Sometimes another sync is requested because a local file is still
        // changing, so wait at least a small amount of time before syncing
        // the folder again.
        scheduleThisFolderSoon();
    }
}

void Folder::slotEmitFinishedDelayed()
{
    emit syncFinished(_syncResult);

    // Immediately check the etag again if there was some sync activity.
    if ((_syncResult.status() == SyncResult::Success
            || _syncResult.status() == SyncResult::Problem)
        && (_syncResult.firstItemDeleted()
               || _syncResult.firstItemNew()
               || _syncResult.firstItemRenamed()
               || _syncResult.firstItemUpdated()
               || _syncResult.firstNewConflictItem())) {
        slotRunEtagJob();
    }
}

// the progress comes without a folder and the valid path set. Add that here
// and hand the result over to the progress dispatcher.
void Folder::slotTransmissionProgress(const ProgressInfo &pi)
{
    emit progressInfo(pi);
    ProgressDispatcher::instance()->setProgressInfo(alias(), pi);
}

// a item is completed: count the errors and forward to the ProgressDispatcher
void Folder::slotItemCompleted(const SyncFileItemPtr &item, ErrorCategory errorCategory)
{
    if (item->_instruction == CSYNC_INSTRUCTION_NONE || item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA) {
        // We only care about the updates that deserve to be shown in the UI
        return;
    }

    if (_silenceErrorsUntilNextSync
        && (item->_status != SyncFileItem::Status::Success && item->_status != SyncFileItem::Status::NoStatus)) {
        item->_errorString.clear();
        item->_status = SyncFileItem::Status::SoftError;
    }

    _syncResult.processCompletedItem(item);

    _fileLog->logItem(*item);
    emit ProgressDispatcher::instance()->itemCompleted(alias(), item, errorCategory);
}

void Folder::slotNewBigFolderDiscovered(const QString &newF, bool isExternal)
{
    const auto newFolder = Utility::trailingSlashPath(newF);
    auto journal = journalDb();

    // Add the entry to the blacklist if it is neither in the blacklist or whitelist already
    bool ok1 = false;
    bool ok2 = false;
    auto blacklist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok1);
    auto whitelist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok2);
    if (ok1 && ok2 && !blacklist.contains(newFolder) && !whitelist.contains(newFolder)) {
        blacklist.append(newFolder);
        journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blacklist);
    }

    // And add the entry to the undecided list and signal the UI
    auto undecidedList = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok1);
    if (ok1) {
        if (!undecidedList.contains(newFolder)) {
            undecidedList.append(newFolder);
            journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, undecidedList);
            emit newBigFolderDiscovered(newFolder);
        }
        QString message = !isExternal ? (tr("A new folder larger than %1 MB has been added: %2.\n")
                                                .arg(ConfigFile().newBigFolderSizeLimit().second)
                                                .arg(newF))
                                      : (tr("A folder from an external storage has been added.\n"));
        message += tr("Please go in the settings to select it if you wish to download it.");

        auto logger = Logger::instance();
        logger->postGuiLog(Theme::instance()->appNameGUI(), message);
    }
}

void Folder::slotExistingFolderNowBig(const QString &folderPath)
{
    const auto trailSlashFolderPath = Utility::trailingSlashPath(folderPath);
    const auto journal = journalDb();
    const auto stopSyncing = ConfigFile().stopSyncingExistingFoldersOverLimit();

    // Add the entry to the whitelist if it is neither in the blacklist or whitelist already
    bool ok1 = false;
    bool ok2 = false;
    auto blacklist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok1);
    auto whitelist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok2);

    const auto inDecidedLists = blacklist.contains(trailSlashFolderPath) || whitelist.contains(trailSlashFolderPath);
    if (inDecidedLists) {
        return;
    }

    auto relevantList = stopSyncing ? blacklist : whitelist;
    const auto relevantListType = stopSyncing ? SyncJournalDb::SelectiveSyncBlackList : SyncJournalDb::SelectiveSyncWhiteList;

    if (ok1 && ok2 && !inDecidedLists) {
        relevantList.append(trailSlashFolderPath);
        journal->setSelectiveSyncList(relevantListType, relevantList);

        if (stopSyncing) {
            // Abort current down sync and start again
            slotTerminateSync();
            scheduleThisFolderSoon();
        }
    }

    auto undecidedListQueryOk = false;
    auto undecidedList = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &undecidedListQueryOk);
    if (undecidedListQueryOk) {
        if (!undecidedList.contains(trailSlashFolderPath)) {
            undecidedList.append(trailSlashFolderPath);
            journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, undecidedList);
            emit newBigFolderDiscovered(trailSlashFolderPath);
        }

        postExistingFolderNowBigNotification(folderPath);
        postExistingFolderNowBigActivity(folderPath);
    }
}

void Folder::postExistingFolderNowBigNotification(const QString &folderPath)
{
    const auto stopSyncing = ConfigFile().stopSyncingExistingFoldersOverLimit();
    const auto messageInstruction =
        stopSyncing ? "Synchronisation of this folder has been disabled." : "Synchronisation of this folder can be disabled in the settings window.";
    const auto message = tr("A folder has surpassed the set folder size limit of %1MB: %2.\n%3")
                             .arg(QString::number(ConfigFile().newBigFolderSizeLimit().second), folderPath, messageInstruction);
    Logger::instance()->postGuiLog(Theme::instance()->appNameGUI(), message);
}

void Folder::postExistingFolderNowBigActivity(const QString &folderPath) const
{
    const auto stopSyncing = ConfigFile().stopSyncingExistingFoldersOverLimit();
    const auto trailSlashFolderPath = Utility::trailingSlashPath(folderPath);

    auto whitelistActivityLink = ActivityLink();
    whitelistActivityLink._label = tr("Keep syncing");
    whitelistActivityLink._primary = false;
    whitelistActivityLink._verb = ActivityLink::WhitelistFolderVerb;

    QVector<ActivityLink> activityLinks = {whitelistActivityLink};

    if (!stopSyncing) {
        auto blacklistActivityLink = ActivityLink();
        blacklistActivityLink._label = tr("Stop syncing");
        blacklistActivityLink._primary = true;
        blacklistActivityLink._verb = ActivityLink::BlacklistFolderVerb;

        activityLinks.append(blacklistActivityLink);
    }

    auto existingFolderNowBigActivity = Activity();
    existingFolderNowBigActivity._type = Activity::NotificationType;
    existingFolderNowBigActivity._dateTime = QDateTime::fromString(QDateTime::currentDateTime().toString(), Qt::ISODate);
    existingFolderNowBigActivity._subject =
        tr("The folder %1 has surpassed the set folder size limit of %2MB.").arg(folderPath, QString::number(ConfigFile().newBigFolderSizeLimit().second));
    existingFolderNowBigActivity._message = tr("Would you like to stop syncing this folder?");
    existingFolderNowBigActivity._accName = _accountState->account()->displayName();
    existingFolderNowBigActivity._folder = alias();
    existingFolderNowBigActivity._file = cleanPath() + '/' + trailSlashFolderPath;
    existingFolderNowBigActivity._links = activityLinks;
    existingFolderNowBigActivity._id = qHash(existingFolderNowBigActivity._file);

    const auto user = UserModel::instance()->findUserForAccount(_accountState.data());
    user->slotAddNotification(this, existingFolderNowBigActivity);
}

void Folder::slotLogPropagationStart()
{
    _fileLog->logLap("Propagation starts");
}

void Folder::slotScheduleThisFolder()
{
    FolderMan::instance()->scheduleFolder(this);
}

void Folder::slotNextSyncFullLocalDiscovery()
{
    _timeSinceLastFullLocalDiscovery.invalidate();
}

void Folder::setSilenceErrorsUntilNextSync(bool silenceErrors)
{
    _silenceErrorsUntilNextSync = silenceErrors;
}

void Folder::schedulePathForLocalDiscovery(const QString &relativePath)
{
    _localDiscoveryTracker->addTouchedPath(relativePath.toUtf8());
}

void Folder::slotFolderConflicts(const QString &folder, const QStringList &conflictPaths)
{
    if (folder != _definition.alias)
        return;
    auto &r = _syncResult;

    // If the number of conflicts is too low, adjust it upwards
    if (conflictPaths.size() > r.numNewConflictItems() + r.numOldConflictItems())
        r.setNumOldConflictItems(conflictPaths.size() - r.numNewConflictItems());
}

void Folder::warnOnNewExcludedItem(const SyncJournalFileRecord &record, const QStringView &path)
{
    // Never warn for items in the database
    if (record.isValid()) {
        return;
    }

    // Don't warn for items that no longer exist.
    // Note: This assumes we're getting file watcher notifications
    // for folders only on creation and deletion - if we got a notification
    // on content change that would create spurious warnings.
    const auto fullPath = QString{_canonicalLocalPath + path};
    if (!FileSystem::fileExists(fullPath)) {
        return;
    }

    bool ok = false;
    const auto selectiveSyncList = _journal.getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    if (!ok) {
        return;
    }
    if (!selectiveSyncList.contains(path + "/")) {
        return;
    }

    QFileInfo excludeItemFileInfo(fullPath);
    const auto excludeItemFilePath = excludeItemFileInfo.filePath();
    const auto message = FileSystem::isDir(fullPath)
        ? tr("The folder %1 was created but was excluded from synchronization previously. "
             "Data inside it will not be synchronized.")
              .arg(excludeItemFilePath)
        : tr("The file %1 was created but was excluded from synchronization previously. "
             "It will not be synchronized.")
              .arg(excludeItemFilePath);

    Logger::instance()->postGuiLog(Theme::instance()->appNameGUI(), message);
}

void Folder::slotWatcherUnreliable(const QString &message)
{
    qCWarning(lcFolder) << "Folder watcher for" << path() << "became unreliable:" << message;
    auto fullMessage =
        tr("Changes in synchronized folders could not be tracked reliably.\n"
           "\n"
           "This means that the synchronization client might not upload local changes "
           "immediately and will instead only scan for local changes and upload them "
           "occasionally (every two hours by default).\n"
           "\n"
           "%1").arg(message);
    Logger::instance()->postGuiLog(Theme::instance()->appNameGUI(), fullMessage);
}

void Folder::slotHydrationStarts()
{
    // // Abort any running full sync run and reschedule
    // if (_engine->isSyncRunning()) {
    //     setSilenceErrorsUntilNextSync(true);
    //     slotTerminateSync();
    //     scheduleThisFolderSoon();
    //     // TODO: This sets the sync state to AbortRequested on done, we don't want that
    // }

    // // Let everyone know we're syncing
    // _syncResult.reset();
    // _syncResult.setStatus(SyncResult::SyncRunning);
    // emit syncStarted();
    // emit syncStateChange();
}

void Folder::slotHydrationDone()
{
    // emit signal to update ui and reschedule normal syncs if necessary
    _syncResult.setStatus(SyncResult::Success);
    emit syncFinished(_syncResult);
    emit syncStateChange();
}

void Folder::slotHydrationFailed(int errorCode, int statusCode, const QString &errorString, const QString &fileName)
{
    _syncResult.setStatus(SyncResult::Error);
    const auto errorMessageDetails = tr("Virtual file download failed with code \"%1\", status \"%2\" and error message \"%3\"")
        .arg(errorCode)
        .arg(statusCode)
        .arg(errorString);
    _syncResult.appendErrorString(errorMessageDetails);

    const auto errorMessageBox = new VfsDownloadErrorDialog(fileName, errorMessageDetails);
    errorMessageBox->setAttribute(Qt::WA_DeleteOnClose);
    errorMessageBox->show();
    errorMessageBox->activateWindow();
    errorMessageBox->raise();
}

void Folder::slotCapabilitiesChanged()
{
    if (_accountState->account()->capabilities().filesLockAvailable()) {
        connect(_folderWatcher.data(), &FolderWatcher::filesLockReleased, this, &Folder::slotFilesLockReleased, Qt::UniqueConnection);
        connect(_folderWatcher.data(), &FolderWatcher::lockedFilesFound, this, &Folder::slotLockedFilesFound, Qt::UniqueConnection);
    }
}

void Folder::scheduleThisFolderSoon()
{
    if (!_scheduleSelfTimer.isActive()) {
        _scheduleSelfTimer.start();
    }
}

void Folder::acceptInvalidFileName(const QString &filePath)
{
    _engine->addAcceptedInvalidFileName(filePath);
}

void Folder::acceptCaseClashConflictFileName(const QString &filePath)
{
    qCInfo(lcFolder) << "going to delete case clash conflict record" << filePath;
    _journal.deleteCaseClashConflictByPathRecord(filePath);

    qCInfo(lcFolder) << "going to delete" << path() + filePath;
    FileSystem::remove(path() + filePath);
}

void Folder::setSaveBackwardsCompatible(bool save)
{
    _saveBackwardsCompatible = save;
}

void Folder::registerFolderWatcher()
{
    if (_folderWatcher)
        return;
    if (!QDir(path()).exists())
        return;

    _folderWatcher.reset(new FolderWatcher(this));
    connect(_folderWatcher.data(), &FolderWatcher::pathChanged,
        this, [this](const QString &path) { slotWatchedPathChanged(path, Folder::ChangeReason::Other); });
    connect(_folderWatcher.data(), &FolderWatcher::lostChanges,
        this, &Folder::slotNextSyncFullLocalDiscovery);
    connect(_folderWatcher.data(), &FolderWatcher::becameUnreliable,
        this, &Folder::slotWatcherUnreliable);
    if (_accountState->account()->capabilities().filesLockAvailable()) {
        connect(_folderWatcher.data(), &FolderWatcher::filesLockReleased, this, &Folder::slotFilesLockReleased);
        connect(_folderWatcher.data(), &FolderWatcher::lockedFilesFound, this, &Folder::slotLockedFilesFound);
    }
    connect(_folderWatcher.data(), &FolderWatcher::filesLockImposed, this, &Folder::slotFilesLockImposed, Qt::UniqueConnection);
    _folderWatcher->init(path());
    _folderWatcher->startNotificatonTest(path() + QLatin1String(".nextcloudsync.log"));
    _folderWatcher->performSetPermissionsTest(path() + QLatin1String(".nextcloudpermissions.log"));
    connect(_engine.data(), &SyncEngine::lockFileDetected, _folderWatcher.data(), &FolderWatcher::slotLockFileDetectedExternally);
}

void Folder::disconnectFolderWatcher()
{
    if (!_folderWatcher) {
        return;
    }
    disconnect(_folderWatcher.data(), &FolderWatcher::pathChanged, nullptr, nullptr);
    disconnect(_folderWatcher.data(), &FolderWatcher::lostChanges, this, &Folder::slotNextSyncFullLocalDiscovery);
    disconnect(_folderWatcher.data(), &FolderWatcher::becameUnreliable, this, &Folder::slotWatcherUnreliable);
    if (_accountState->account()->capabilities().filesLockAvailable()) {
        disconnect(_folderWatcher.data(), &FolderWatcher::filesLockReleased, this, &Folder::slotFilesLockReleased);
        disconnect(_folderWatcher.data(), &FolderWatcher::lockedFilesFound, this, &Folder::slotLockedFilesFound);
    }
    disconnect(_folderWatcher.data(), &FolderWatcher::filesLockImposed, this, &Folder::slotFilesLockImposed);
}

bool Folder::virtualFilesEnabled() const
{
    return _definition.virtualFilesMode != Vfs::Off && !isVfsOnOffSwitchPending() && !_vfs.isNull();
}

void Folder::slotAboutToRemoveAllFiles(SyncFileItem::Direction dir, std::function<void(bool)> callback)
{
    const auto isDownDirection = (dir == SyncFileItem::Down);
    const QString msg = isDownDirection ? tr("A large number of files in the server have been deleted.\nPlease confirm if you'd like to proceed with these deletions.\nAlternatively, you can restore all deleted files by uploading from '%1' folder to the server.")
                                        : tr("A large number of files in your local '%1' folder have been deleted.\nPlease confirm if you'd like to proceed with these deletions.\nAlternatively, you can restore all deleted files by downloading them from the server.");
    auto msgBox = new QMessageBox(QMessageBox::Warning, tr("Remove all files?"),
                                  msg.arg(shortGuiLocalPath()), QMessageBox::NoButton);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->addButton(tr("Proceed with Deletion"), QMessageBox::DestructiveRole);
    QPushButton *keepBtn = msgBox->addButton(isDownDirection ? tr("Restore Files to Server") : tr("Restore Files from Server"),
                                             QMessageBox::AcceptRole);
    bool oldPaused = syncPaused();
    setSyncPaused(true);
    connect(msgBox, &QMessageBox::finished, this, [msgBox, keepBtn, callback, oldPaused, this] {
        const bool cancel = msgBox->clickedButton() == keepBtn;
        callback(cancel);
        if (cancel) {
            FileSystem::setFolderMinimumPermissions(path());
            journalDb()->clearFileTable();
            _lastEtag.clear();
            slotScheduleThisFolder();
        }
        setSyncPaused(oldPaused);
    });
    connect(this, &Folder::destroyed, msgBox, &QMessageBox::deleteLater);
    msgBox->open();
}

void Folder::removeLocalE2eFiles()
{
    qCDebug(lcFolder) << "Removing local E2EE files";

    const QDir folderRootDir(path());
    QStringList e2eFoldersToBlacklist;
    const auto couldGetFiles = _journal.getFilesBelowPath("", [this, &e2eFoldersToBlacklist, &folderRootDir](const SyncJournalFileRecord &rec) {
        // We only want to add the root-most encrypted folder to the blacklist
        if (rec.isValid() && rec.isE2eEncrypted() && rec.isDirectory()) {
            QDir pathDir(_canonicalLocalPath + rec.path());
            bool parentPathEncrypted = false;

            while (pathDir.cdUp() && pathDir != folderRootDir) {
                SyncJournalFileRecord dirRec;
                const auto currentCanonicalPath = pathDir.canonicalPath();

                if (!_journal.getFileRecord(currentCanonicalPath, &dirRec)) {
                    qCWarning(lcFolder) << "Failed to get file record for" << currentCanonicalPath;
                }

                if (dirRec.isE2eEncrypted()) {
                    parentPathEncrypted = true;
                    break;
                }
            }

            if (!parentPathEncrypted) {
                const auto pathAdjusted = Utility::trailingSlashPath(rec._path);
                e2eFoldersToBlacklist.append(pathAdjusted);
            }
        }
    });

    if (!couldGetFiles) {
        qCWarning(lcFolder) << "Could not fetch E2EE folders to blacklist in this folder:" << path();
        return;
    } else if (e2eFoldersToBlacklist.isEmpty()) {
        qCWarning(lcFolder) << "No E2EE folders found at path" << path();
        return;
    }

    qCInfo(lcFolder) << "About to blacklist: " << e2eFoldersToBlacklist;

    bool ok = false;
    const auto existingBlacklist = _journal.getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    Q_ASSERT(ok);

    const auto existingBlacklistSet = QSet<QString>{existingBlacklist.begin(), existingBlacklist.end()};
    auto expandedBlacklistSet = QSet<QString>{existingBlacklist.begin(), existingBlacklist.end()};

    for (const auto &path : std::as_const(e2eFoldersToBlacklist)) {
        expandedBlacklistSet.insert(path);
    }

    // same as in void FolderStatusModel::slotApplySelectiveSync()
    // only start sync if blackList has changed
    // database lists will get updated during discovery
    const auto changes = (existingBlacklistSet - expandedBlacklistSet) + (expandedBlacklistSet - existingBlacklistSet);

    _journal.setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, expandedBlacklistSet.values());
    _journal.setSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, changes.values());

    if (!changes.isEmpty()) {
        _journal.setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, QStringList());
        if (isBusy()) {
            slotTerminateSync();
        }
        for (const auto &it : changes) {
            _journal.schedulePathForRemoteDiscovery(it);
            schedulePathForLocalDiscovery(it);
        }
        FolderMan::instance()->scheduleFolderForImmediateSync(this);
    }
}

QString Folder::fileFromLocalPath(const QString &localPath) const
{
    return localPath.mid(cleanPath().length() + 1);
}

void FolderDefinition::save(QSettings &settings, const FolderDefinition &folder)
{
    settings.setValue(QLatin1String("localPath"), folder.localPath);
    settings.setValue(QLatin1String("journalPath"), folder.journalPath);
    settings.setValue(QLatin1String("targetPath"), folder.targetPath);
    settings.setValue(QLatin1String("paused"), folder.paused);
    settings.setValue(QLatin1String("ignoreHiddenFiles"), folder.ignoreHiddenFiles);

    settings.setValue(QStringLiteral("virtualFilesMode"), Vfs::modeToString(folder.virtualFilesMode));

    // Ensure new vfs modes won't be attempted by older clients
    if (folder.virtualFilesMode == Vfs::WindowsCfApi) {
        settings.setValue(QLatin1String(versionC), 3);
    } else {
        settings.setValue(QLatin1String(versionC), 2);
    }

    // Happens only on Windows when the explorer integration is enabled.
    if (!folder.navigationPaneClsid.isNull())
        settings.setValue(QLatin1String("navigationPaneClsid"), folder.navigationPaneClsid);
    else
        settings.remove(QLatin1String("navigationPaneClsid"));

    // macOS sandbox: persist security-scoped bookmark data
    if (!folder.securityScopedBookmarkData.isEmpty())
        settings.setValue(QLatin1String("securityScopedBookmarkData"), folder.securityScopedBookmarkData);
    else
        settings.remove(QLatin1String("securityScopedBookmarkData"));
}

bool FolderDefinition::load(QSettings &settings, const QString &alias,
    FolderDefinition *folder)
{
    folder->alias = FolderMan::unescapeAlias(alias);
    folder->localPath = settings.value(QLatin1String("localPath")).toString();
    folder->journalPath = settings.value(QLatin1String("journalPath")).toString();
    folder->targetPath = settings.value(QLatin1String("targetPath")).toString();
    folder->paused = settings.value(QLatin1String("paused")).toBool();
    folder->ignoreHiddenFiles = settings.value(QLatin1String("ignoreHiddenFiles"), QVariant(true)).toBool();
    folder->navigationPaneClsid = settings.value(QLatin1String("navigationPaneClsid")).toUuid();

    folder->virtualFilesMode = Vfs::Off;
    QString vfsModeString = settings.value(QStringLiteral("virtualFilesMode")).toString();
    if (!vfsModeString.isEmpty()) {
        if (auto mode = Vfs::modeFromString(vfsModeString)) {
            folder->virtualFilesMode = *mode;
        } else {
            qCWarning(lcFolder) << "Unknown virtualFilesMode:" << vfsModeString << "assuming 'off'";
        }
    } else {
        if (settings.value(QLatin1String("usePlaceholders")).toBool()) {
            folder->virtualFilesMode = Vfs::WithSuffix;
            folder->upgradeVfsMode = true; // maybe winvfs is available?
        }
    }

    // macOS sandbox: load security-scoped bookmark data
    folder->securityScopedBookmarkData = settings.value(QLatin1String("securityScopedBookmarkData")).toByteArray();

    // Old settings can contain paths with native separators. In the rest of the
    // code we assume /, so clean it up now.
    folder->localPath = prepareLocalPath(folder->localPath);

    // Target paths also have a convention
    folder->targetPath = prepareTargetPath(folder->targetPath);

    return true;
}

QString FolderDefinition::prepareLocalPath(const QString &path)
{
    const auto normalisedPath = QDir::fromNativeSeparators(path);
    return Utility::trailingSlashPath(normalisedPath);
}

QString FolderDefinition::prepareTargetPath(const QString &path)
{
    QString p = path;
    if (p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    // Doing this second ensures the empty string or "/" come
    // out as "/".
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    return p;
}

QString FolderDefinition::absoluteJournalPath() const
{
    return QDir(localPath).filePath(journalPath);
}

QString FolderDefinition::defaultJournalPath(AccountPtr account)
{
    return SyncJournalDb::makeDbName(localPath, account->url(), targetPath, account->credentials()->user());
}

} // namespace OCC
