/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "folder.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "application.h"
#include "common/checksums.h"
#include "common/depreaction.h"
#include "common/filesystembase.h"
#include "common/syncjournalfilerecord.h"
#include "common/version.h"
#include "common/vfs.h"
#include "configfile.h"
#include "filesystem.h"
#include "folderman.h"
#include "folderwatcher.h"
#include "libsync/graphapi/spacesmanager.h"
#include "localdiscoverytracker.h"
#include "networkjobs.h"
#include "scheduling/syncscheduler.h"
#include "settingsdialog.h"
#include "socketapi/socketapi.h"
#include "syncengine.h"
#include "syncresult.h"
#include "syncrunfilelog.h"
#include "theme.h"

#ifdef Q_OS_WIN
#include "common/utility_win.h"
#endif

#include <QTimer>
#include <QUrl>
#include <QDir>
#include <QSettings>

#include <QMessageBox>
#include <QPushButton>
#include <QApplication>

using namespace std::chrono_literals;

namespace {

/*
 * [Accounts]
 * 1\Folders\4\version=2
 * 1\FoldersWithPlaceholders\3\version=3
 */
auto versionC()
{
    return QLatin1String("version");
}

auto davUrlC()
{
    return QStringLiteral("davUrl");
}

auto spaceIdC()
{
    return QStringLiteral("spaceId");
}

auto displayNameC()
{
    return QLatin1String("displayString");
}

auto deployedC()
{
    return QStringLiteral("deployed");
}

auto priorityC()
{
    return QStringLiteral("priority");
}
}

namespace OCC {

using namespace FileSystem::SizeLiterals;

Q_LOGGING_CATEGORY(lcFolder, "gui.folder", QtInfoMsg)

Folder::Folder(const FolderDefinition &definition,
    const AccountStatePtr &accountState, std::unique_ptr<Vfs> &&vfs,
    QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _definition(definition)
    , _lastSyncDuration(0)
    , _consecutiveFailingSyncs(0)
    , _consecutiveFollowUpSyncs(0)
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
    setSyncState(status);
    // check if the local path exists
    if (checkLocalPath()) {
        prepareFolder(path());
        // those errors should not persist over sessions
        _journal.wipeErrorBlacklistCategory(SyncJournalErrorBlacklistRecord::Category::LocalSoftError);
        _engine.reset(new SyncEngine(_accountState->account(), webDavUrl(), path(), remotePath(), &_journal));
        // pass the setting if hidden files are to be ignored, will be read in csync_update
        _engine->setIgnoreHiddenFiles(_definition.ignoreHiddenFiles);

        if (!_engine->loadDefaultExcludes()) {
            qCWarning(lcFolder, "Could not read system exclude file");
        }

        connect(_accountState.data(), &AccountState::isConnectedChanged, this, &Folder::canSyncChanged);

        connect(_engine.data(), &SyncEngine::started, this, &Folder::slotSyncStarted, Qt::QueuedConnection);
        connect(_engine.data(), &SyncEngine::finished, this, &Folder::slotSyncFinished, Qt::QueuedConnection);

        connect(_engine.data(), &SyncEngine::aboutToRemoveAllFiles,
            this, &Folder::slotAboutToRemoveAllFiles);
        connect(_engine.data(), &SyncEngine::transmissionProgress, this, [this](const ProgressInfo &pi) {
            emit ProgressDispatcher::instance()->progressInfo(this, pi);
        });
        connect(_engine.data(), &SyncEngine::itemCompleted,
            this, &Folder::slotItemCompleted);
        connect(_engine.data(), &SyncEngine::newBigFolder,
            this, &Folder::slotNewBigFolderDiscovered);
        connect(_engine.data(), &SyncEngine::seenLockedFile, FolderMan::instance(), &FolderMan::slotSyncOnceFileUnlocks);
        connect(_engine.data(), &SyncEngine::aboutToPropagate,
            this, &Folder::slotLogPropagationStart);
        connect(_engine.data(), &SyncEngine::syncError, this, &Folder::slotSyncError);

        connect(ProgressDispatcher::instance(), &ProgressDispatcher::folderConflicts,
            this, &Folder::slotFolderConflicts);
        connect(_engine.data(), &SyncEngine::excluded, this, [this](const QString &path) { Q_EMIT ProgressDispatcher::instance()->excluded(this, path); });

        _localDiscoveryTracker.reset(new LocalDiscoveryTracker);
        connect(_engine.data(), &SyncEngine::finished,
            _localDiscoveryTracker.data(), &LocalDiscoveryTracker::slotSyncFinished);
        connect(_engine.data(), &SyncEngine::itemCompleted,
            _localDiscoveryTracker.data(), &LocalDiscoveryTracker::slotItemCompleted);

        // Potentially upgrade suffix vfs to windows vfs
        OC_ENFORCE(_vfs);
        if (_definition.virtualFilesMode == Vfs::WithSuffix
            && _definition.upgradeVfsMode) {
            if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
                if (auto winvfs = VfsPluginManager::instance().createVfsFromPlugin(Vfs::WindowsCfApi)) {
                    // Wipe the existing suffix files from fs and journal
                    SyncEngine::wipeVirtualFiles(path(), _journal, *_vfs);

                    // Then switch to winvfs mode
                    _vfs.reset(winvfs.release());
                    _definition.virtualFilesMode = Vfs::WindowsCfApi;
                }
            }
            saveToSettings();
        }
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
}

bool Folder::checkLocalPath()
{
#ifdef Q_OS_WIN
    Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
    const QFileInfo fi(_definition.localPath());
    _canonicalLocalPath = fi.canonicalFilePath();
#ifdef Q_OS_MAC
    // Workaround QTBUG-55896  (Should be fixed in Qt 5.8)
    _canonicalLocalPath = _canonicalLocalPath.normalized(QString::NormalizationForm_C);
#endif
    if (_canonicalLocalPath.isEmpty()) {
        qCWarning(lcFolder) << "Broken symlink:" << _definition.localPath();
        _canonicalLocalPath = _definition.localPath();
    } else if (!_canonicalLocalPath.endsWith(QLatin1Char('/'))) {
        _canonicalLocalPath.append(QLatin1Char('/'));
    }

    QString error;
    if (fi.isDir() && fi.isReadable() && fi.isWritable()) {
#ifdef Q_OS_WIN
        if (_canonicalLocalPath.size() > MAX_PATH) {
            if (!FileSystem::longPathsEnabledOnWindows()) {
                error =
                    tr("The path '%1' is too long. Please enable long paths in the Windows settings or choose a different folder.").arg(_canonicalLocalPath);
            }
        }
#endif

        if (error.isEmpty()) {
            qCDebug(lcFolder) << "Checked local path ok";
            if (!_journal.open()) {
                error = tr("%1 failed to open the database.").arg(_definition.localPath());
            }
        }
    } else {
        // Check directory again
        if (!FileSystem::fileExists(_definition.localPath(), fi)) {
            error = tr("Local folder %1 does not exist.").arg(_definition.localPath());
        } else if (!fi.isDir()) {
            error = tr("%1 should be a folder but is not.").arg(_definition.localPath());
        } else if (!fi.isReadable()) {
            error = tr("%1 is not readable.").arg(_definition.localPath());
        } else if (!fi.isWritable()) {
            error = tr("%1 is not writable.").arg(_definition.localPath());
        }
    }
    if (!error.isEmpty()) {
        qCWarning(lcFolder) << error;
        _syncResult.appendErrorString(error);
        setSyncState(SyncResult::SetupError);
        return false;
    }
    return true;
}

SyncOptions Folder::loadSyncOptions()
{
    SyncOptions opt(_vfs);
    ConfigFile cfgFile;

    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    opt._newBigFolderSizeLimit = newFolderLimit.first ? newFolderLimit.second * 1000LL * 1000LL : -1; // convert from MB to B
    opt._confirmExternalStorage = cfgFile.confirmExternalStorage();
    opt._moveFilesToTrash = cfgFile.moveToTrash();
    opt._vfs = _vfs;
    opt._parallelNetworkJobs = _accountState->account()->isHttp2Supported() ? 20 : 6;

    opt._initialChunkSize = cfgFile.chunkSize();
    opt._minChunkSize = cfgFile.minChunkSize();
    opt._maxChunkSize = cfgFile.maxChunkSize();
    opt._targetChunkUploadDuration = cfgFile.targetChunkUploadDuration();

    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();
    return opt;
}

void Folder::prepareFolder(const QString &path)
{
#ifdef Q_OS_WIN
    // First create a Desktop.ini so that the folder and favorite link show our application's icon.
    const QFileInfo desktopIniPath{QStringLiteral("%1/Desktop.ini").arg(path)};
    {
        const QString updateIconKey = QStringLiteral("%1/UpdateIcon").arg(Theme::instance()->appName());
        QSettings desktopIni(desktopIniPath.absoluteFilePath(), QSettings::IniFormat);
        if (desktopIni.value(updateIconKey, true).toBool()) {
            qCInfo(lcFolder) << "Creating" << desktopIni.fileName() << "to set a folder icon in Explorer.";
            desktopIni.setValue(QStringLiteral(".ShellClassInfo/IconResource"), QDir::toNativeSeparators(qApp->applicationFilePath()));
            desktopIni.setValue(updateIconKey, true);
        } else {
            qCInfo(lcFolder) << "Skip icon update for" << desktopIni.fileName() << "," << updateIconKey << "is disabled";
        }

        desktopIni.sync();
    }

    const QString longFolderPath = FileSystem::longWinPath(path);
    const QString longDesktopIniPath = FileSystem::longWinPath(desktopIniPath.absoluteFilePath());
    // Set the folder as system and Desktop.ini as hidden+system for explorer to pick it.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/cc144102
    const DWORD folderAttrs = GetFileAttributesW(reinterpret_cast<const wchar_t *>(longFolderPath.utf16()));
    if (!SetFileAttributesW(reinterpret_cast<const wchar_t *>(longFolderPath.utf16()), folderAttrs | FILE_ATTRIBUTE_SYSTEM)) {
        const auto error = GetLastError();
        qCWarning(lcFolder) << "SetFileAttributesW failed on" << longFolderPath << Utility::formatWinError(error);
    }
    if (!SetFileAttributesW(reinterpret_cast<const wchar_t *>(longDesktopIniPath.utf16()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
        const auto error = GetLastError();
        qCWarning(lcFolder) << "SetFileAttributesW failed on" << longDesktopIniPath << Utility::formatWinError(error);
    }
#endif
}

QByteArray Folder::id() const
{
    return _definition.id();
}

QString Folder::displayName() const
{
    return _definition.displayName();
}

QString Folder::path() const
{
    return _canonicalLocalPath;
}

QString Folder::shortGuiLocalPath() const
{
    QString p = _definition.localPath();
    QString home = QDir::homePath();
    if (!home.endsWith(QLatin1Char('/'))) {
        home.append(QLatin1Char('/'));
    }
    if (p.startsWith(home)) {
        p = p.mid(home.length());
    }
    if (p.length() > 1 && p.endsWith(QLatin1Char('/'))) {
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

    if (cleanedPath.length() == 3 && cleanedPath.endsWith(QLatin1String(":/")))
        cleanedPath.remove(2, 1);

    return cleanedPath;
}

bool Folder::isSyncRunning() const
{
    return !hasSetupError() && _engine->isSyncRunning();
}

QString Folder::remotePath() const
{
    return _definition.targetPath();
}

QUrl Folder::webDavUrl() const
{
    const QString spaceId = _definition.spaceId();
    if (!spaceId.isEmpty()) {
        if (auto *space = _accountState->account()->spacesManager()->space(spaceId)) {
            return QUrl(space->drive().getRoot().getWebDavUrl());
        }
    }
    return _definition.webDavUrl();
}

QString Folder::remotePathTrailingSlash() const
{
    const QString remote = remotePath();
    if (remote == QLatin1Char('/')) {
        return remote;
    }
    Q_ASSERT(!remote.endsWith(QLatin1Char('/')));
    return remote + QLatin1Char('/');
}

QUrl Folder::remoteUrl() const
{
    return Utility::concatUrlPath(webDavUrl(), remotePath());
}

bool Folder::syncPaused() const
{
    return _definition.paused;
}

bool Folder::canSync() const
{
    return !syncPaused() && accountState()->isConnected() && isReady() && _accountState->account()->hasCapabilities() && _folderWatcher;
}

bool Folder::isReady() const
{
    return _vfsIsReady;
}

void Folder::setSyncPaused(bool paused)
{
    if (hasSetupError()) {
        return;
    }
    if (paused == _definition.paused) {
        return;
    }

    _definition.paused = paused;
    saveToSettings();

    emit syncPausedChanged(this, paused);
    if (!paused) {
        setSyncState(SyncResult::NotYetStarted);
    } else {
        setSyncState(SyncResult::Paused);
    }
    emit canSyncChanged();
}

void Folder::setSyncState(SyncResult::Status state)
{
    if (state != _syncResult.status()) {
        _syncResult.setStatus(state);
        Q_EMIT syncStateChange();
    }
}

SyncResult Folder::syncResult() const
{
    return _syncResult;
}

void Folder::prepareToSync()
{
    setSyncState(SyncResult::NotYetStarted);
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

    qCInfo(lcFolder) << "Folder" << path() << "sync result: " << _syncResult.status();
}

void Folder::createGuiLog(const QString &filename, LogStatus status, int count,
    const QString &renameTarget)
{
    if (count > 0) {
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
        }

        if (!text.isEmpty()) {
            ocApp()->gui()->slotShowOptionalTrayMessage(tr("Sync Activity"), text);
        }
    }
}

void Folder::startVfs()
{
    OC_ENFORCE(_vfs);
    OC_ENFORCE(_vfs->mode() == _definition.virtualFilesMode);

    const auto result = Vfs::checkAvailability(path(), _vfs->mode());
    if (!result) {
        _syncResult.appendErrorString(result.error());
        setSyncState(SyncResult::SetupError);
        return;
    }

    VfsSetupParams vfsParams(_accountState->account(), webDavUrl(), groupInSidebar(), _engine.get());
    vfsParams.filesystemPath = path();
    vfsParams.remotePath = remotePathTrailingSlash();
    vfsParams.journal = &_journal;
    vfsParams.providerDisplayName = Theme::instance()->appNameGUI();
    vfsParams.providerName = Theme::instance()->appName();
    vfsParams.providerVersion = Version::version();
    vfsParams.multipleAccountsRegistered = AccountManager::instance()->accounts().size() > 1;

    connect(&_engine->syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
        _vfs.data(), &Vfs::fileStatusChanged);


    connect(_vfs.data(), &Vfs::started, this, [this] {
        // Immediately mark the sqlite temporaries as excluded. They get recreated
        // on db-open and need to get marked again every time.
        QString stateDbFile = _journal.databaseFilePath();
        _vfs->fileStatusChanged(stateDbFile + QStringLiteral("-wal"), SyncFileStatus::StatusExcluded);
        _vfs->fileStatusChanged(stateDbFile + QStringLiteral("-shm"), SyncFileStatus::StatusExcluded);
        _engine->setSyncOptions(loadSyncOptions());
        registerFolderWatcher();
        _vfsIsReady = true;
        Q_EMIT FolderMan::instance()->folderListChanged();
        // we are setup, schedule ourselves if we can
        // if not the scheduler will take care of it later.
        if (canSync()) {
            FolderMan::instance()->scheduler()->enqueueFolder(this);
        }
    });
    connect(_vfs.data(), &Vfs::error, this, [this](const QString &error) {
        _syncResult.appendErrorString(error);
        setSyncState(SyncResult::SetupError);
        _vfsIsReady = false;
    });

    _vfs->start(vfsParams);
}

int Folder::slotDiscardDownloadProgress()
{
    // Delete from journal and from filesystem.
    QDir folderpath(_definition.localPath());
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

void Folder::slotWatchedPathsChanged(const QSet<QString> &paths, ChangeReason reason)
{
    Q_ASSERT(isReady());
    bool needSync = false;
    for (const auto &path : paths) {
        Q_ASSERT(FileSystem::isChildPathOf(path, this->path()));

        const QString relativePath = path.mid(this->path().size());
        if (reason == ChangeReason::UnLock) {
            journalDb()->wipeErrorBlacklistEntry(relativePath, SyncJournalErrorBlacklistRecord::Category::LocalSoftError);

            {
                // horrible hack to compensate that we don't handle folder deletes on a per file basis
                int index = 0;
                QString p = relativePath;
                while ((index = p.lastIndexOf(QLatin1Char('/'))) != -1) {
                    p = p.left(index);
                    const auto rec = journalDb()->errorBlacklistEntry(p);
                    if (rec.isValid()) {
                        if (rec._errorCategory == SyncJournalErrorBlacklistRecord::Category::LocalSoftError) {
                            journalDb()->wipeErrorBlacklistEntry(p);
                        }
                    }
                }
            }
        }

        // Add to list of locally modified paths
        //
        // We do this before checking for our own sync-related changes to make
        // extra sure to not miss relevant changes.
        _localDiscoveryTracker->addTouchedPath(relativePath);

        SyncJournalFileRecord record;
        _journal.getFileRecord(relativePath.toUtf8(), &record);
        if (reason != ChangeReason::UnLock) {
            // Check that the mtime/size actually changed or there was
            // an attribute change (pin state) that caused the notification
            bool spurious = false;
            if (record.isValid() && !FileSystem::fileChanged(QFileInfo{path}, record._fileSize, record._modtime, record._inode)) {
                spurious = true;

                if (auto pinState = _vfs->pinState(relativePath)) {
                    if (*pinState == PinState::AlwaysLocal && record.isVirtualFile())
                        spurious = false;
                    if (*pinState == PinState::OnlineOnly && record.isFile())
                        spurious = false;
                }
            }
            if (spurious) {
                qCInfo(lcFolder) << "Ignoring spurious notification for file" << relativePath;
                Q_ASSERT([&] {
                    Q_ASSERT(record.isValid());
                    // we don't intend to burn to many cpu cycles so limit this check on small files
                    if (!record.isVirtualFile() && record._fileSize < 1_mb) {
                        const auto header = ChecksumHeader::parseChecksumHeader(record._checksumHeader);
                        auto *compute = new ComputeChecksum(this);
                        compute->setChecksumType(header.type());
                        quint64 inode = 0;
                        FileSystem::getInode(path, &inode);
                        connect(compute, &ComputeChecksum::done, this, [=](CheckSums::Algorithm checksumType, const QByteArray &checksum) {
                            compute->deleteLater();
                            qWarning() << "Spurious notification:" << path << (checksum == header.checksum()) << checksum << header.checksum()
                                       << "Inode:" << record._inode << inode;
                            Q_ASSERT(inode == record._inode);
                            Q_ASSERT(checksum == header.checksum());
                        });
                        compute->start(path);
                    }
                    return true;
                }());

                continue; // probably a spurious notification
            }
        }
        warnOnNewExcludedItem(record, relativePath);

        emit watchedFileChangedExternally(path);
        needSync = true;
    }
    if (needSync) {
        FolderMan::instance()->scheduler()->enqueueFolder(this);
    }
}

void Folder::implicitlyHydrateFile(const QString &relativepath)
{
    qCInfo(lcFolder) << "Implicitly hydrate virtual file:" << relativepath;

    // Set in the database that we should download the file
    SyncJournalFileRecord record;
    _journal.getFileRecord(relativepath.toUtf8(), &record);
    if (!record.isValid()) {
        qCInfo(lcFolder) << "Did not find file in db";
        return;
    }
    if (!record.isVirtualFile()) {
        qCInfo(lcFolder) << "The file is not virtual";
        return;
    }
    record._type = ItemTypeVirtualFileDownload;
    _journal.setFileRecord(record);

    // Change the file's pin state if it's contradictory to being hydrated
    // (suffix-virtual file's pin state is stored at the hydrated path)
    const auto pin = _vfs->pinState(relativepath);
    if (pin && *pin == PinState::OnlineOnly) {
        _vfs->setPinState(relativepath, PinState::Unspecified);
    }

    // Add to local discovery
    schedulePathForLocalDiscovery(relativepath);
    FolderMan::instance()->scheduler()->enqueueFolder(this);
}

void Folder::setVirtualFilesEnabled(bool enabled)
{
    Vfs::Mode newMode = _definition.virtualFilesMode;
    if (enabled && _definition.virtualFilesMode == Vfs::Off) {
        newMode = VfsPluginManager::instance().bestAvailableVfsMode();
    } else if (!enabled && _definition.virtualFilesMode != Vfs::Off) {
        newMode = Vfs::Off;
    }

    if (newMode != _definition.virtualFilesMode) {
        _vfs->stop();
        _vfs->unregisterFolder();

        disconnect(_vfs.data(), nullptr, this, nullptr);
        disconnect(&_engine->syncFileStatusTracker(), nullptr, _vfs.data(), nullptr);

        _vfsIsReady = false;
        _vfs.reset(VfsPluginManager::instance().createVfsFromPlugin(newMode).release());

        _definition.virtualFilesMode = newMode;
        startVfs();
        saveToSettings();
    }
}

bool Folder::supportsSelectiveSync() const
{
    return !virtualFilesEnabled() && !isVfsOnOffSwitchPending();
}

bool Folder::isDeployed() const
{
    return _definition.isDeployed();
}

void Folder::saveToSettings() const
{
    // Remove first to make sure we don't get duplicates
    removeFromSettings();

    auto settings = _accountState->settings();
    settings->beginGroup(QStringLiteral("Folders"));

    auto definitionToSave = _definition;

    // migration
    if (accountState()->supportsSpaces() && _definition.spaceId().isEmpty()) {
        OC_DISABLE_DEPRECATED_WARNING
        if (auto *space = accountState()->account()->spacesManager()->spaceByUrl(webDavUrl())) {
            OC_ENABLE_DEPRECATED_WARNING
            definitionToSave.setSpaceId(space->drive().getRoot().getId());
        }
    }
    // with spaces we rely on the space id
    // we save the dav url nevertheless to have it available during startup
    definitionToSave.setWebDavUrl(webDavUrl());

    // Note: Each of these groups might have a "version" tag, but that's
    //       currently unused.
    settings->beginGroup(QString::fromUtf8(definitionToSave.id()));
    FolderDefinition::save(*settings, definitionToSave);

    settings->sync();
    qCInfo(lcFolder) << "Saved folder" << definitionToSave.localPath() << "to settings, status" << settings->status();
}

void Folder::removeFromSettings() const
{
    auto settings = _accountState->settings();
    const QString id = QString::fromUtf8(_definition.id());
    settings->beginGroup(QStringLiteral("Folders"));
    settings->remove(id);
    settings->endGroup();
    settings->beginGroup(QStringLiteral("Multifolders"));
    settings->remove(id);
    settings->endGroup();
    settings->beginGroup(QStringLiteral("FoldersWithPlaceholders"));
    settings->remove(id);
}

bool Folder::isFileExcludedAbsolute(const QString &fullPath) const
{
    if (OC_ENSURE_NOT(_engine.isNull())) {
        return _engine->isExcluded(fullPath);
    }
    return true;
}

bool Folder::isFileExcludedRelative(const QString &relativePath) const
{
    return isFileExcludedAbsolute(path() + relativePath);
}

void Folder::slotTerminateSync()
{
    if (isReady()) {
        qCInfo(lcFolder) << "folder " << path() << " Terminating!";
        if (_engine->isSyncRunning()) {
            _engine->abort();
            setSyncState(SyncResult::SyncAbortRequested);
        }
    }
}

void Folder::wipeForRemoval()
{
    // we can't acces those variables
    if (hasSetupError()) {
        return;
    }
    // prevent interaction with the db etc
    _vfsIsReady = false;

    // stop reacting to changes
    // especially the upcoming deletion of the db
    _folderWatcher.reset();

    // Delete files that have been partially downloaded.
    slotDiscardDownloadProgress();

    // Unregister the socket API so it does not keep the .sync_journal file open
    FolderMan::instance()->socketApi()->slotUnregisterPath(this);
    _journal.close(); // close the sync journal

    // Remove db and temporaries
    const QString stateDbFile = _engine->journal()->databaseFilePath();

    QFile file(stateDbFile);
    if (file.exists()) {
        if (!file.remove()) {
            qCCritical(lcFolder) << "Failed to remove existing csync StateDB " << stateDbFile;
        } else {
            qCInfo(lcFolder) << "wipe: Removed csync StateDB " << stateDbFile;
        }
    } else {
        qCWarning(lcFolder) << "statedb is empty, can not remove.";
    }

    // Also remove other db related files
    QFile::remove(stateDbFile + QStringLiteral(".ctmp"));
    QFile::remove(stateDbFile + QStringLiteral("-shm"));
    QFile::remove(stateDbFile + QStringLiteral("-wal"));
    QFile::remove(stateDbFile + QStringLiteral("-journal"));

    _vfs->stop();
    _vfs->unregisterFolder();
    _vfs.reset(nullptr); // warning: folder now in an invalid state
}

bool Folder::reloadExcludes()
{
    if (!_engine) {
        return true;
    }
    return _engine->reloadExcludes();
}

void Folder::startSync()
{
    Q_ASSERT(isReady());
    Q_ASSERT(_folderWatcher);

    if (!OC_ENSURE(!isSyncRunning())) {
        qCCritical(lcFolder) << "ERROR csync is still running and new sync requested.";
        return;
    }

    _timeSinceLastSyncStart.start();
    setSyncState(SyncResult::SyncPrepare);
    _syncResult.reset();

    qCInfo(lcFolder) << "*** Start syncing " << remoteUrl().toString() << "client version"
                     << Theme::instance()->aboutVersions(Theme::VersionFormat::OneLiner);

    _fileLog->start(path());

    if (!reloadExcludes()) {
        slotSyncError(tr("Could not read system exclude file"));
        QMetaObject::invokeMethod(
            this, [this] { slotSyncFinished(false); }, Qt::QueuedConnection);
        return;
    }

    setDirtyNetworkLimits();

    const std::chrono::milliseconds fullLocalDiscoveryInterval = ConfigFile().fullLocalDiscoveryInterval();
    const bool hasDoneFullLocalDiscovery = _timeSinceLastFullLocalDiscovery.isValid();
    // negative fullLocalDiscoveryInterval means we don't require periodic full runs
    const bool periodicFullLocalDiscoveryNow =
        fullLocalDiscoveryInterval.count() >= 0 && _timeSinceLastFullLocalDiscovery.hasExpired(fullLocalDiscoveryInterval.count());
    if (_folderWatcher && _folderWatcher->isReliable()
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
    if (_allowRemoveAllOnce) {
        _engine->setPromtRemoveAllFiles(false);
        _allowRemoveAllOnce = false;
    } else {
        _engine->setPromtRemoveAllFiles(ConfigFile().promptDeleteFiles());
    }

    QMetaObject::invokeMethod(_engine.data(), &SyncEngine::startSync, Qt::QueuedConnection);

    emit syncStarted();
}

void Folder::setDirtyNetworkLimits()
{
    Q_ASSERT(isReady());
    ConfigFile cfg;
    int downloadLimit = -75; // 75%
    int useDownLimit = cfg.useDownloadLimit();
    if (useDownLimit >= 1) {
        downloadLimit = cfg.downloadLimit() * 1000;
    } else if (useDownLimit == 0) {
        downloadLimit = 0;
    }

    int uploadLimit = -75; // 75%
    int useUpLimit = cfg.useUploadLimit();
    if (useUpLimit >= 1) {
        uploadLimit = cfg.uploadLimit() * 1000;
    } else if (useUpLimit == 0) {
        uploadLimit = 0;
    }

    _engine->setNetworkLimits(uploadLimit, downloadLimit);
}

void Folder::slotSyncError(const QString &message, ErrorCategory category)
{
    _syncResult.appendErrorString(message);
    emit ProgressDispatcher::instance()->syncError(this, message, category);
}

void Folder::slotSyncStarted()
{
    qCInfo(lcFolder) << "#### Propagation start ####################################################";
    setSyncState(SyncResult::SyncRunning);
}

void Folder::slotSyncFinished(bool success)
{
    if (!isReady()) {
        // probably removing the folder
        return;
    }
    qCInfo(lcFolder) << "Client version" << Theme::instance()->aboutVersions(Theme::VersionFormat::OneLiner);

    bool syncError = !_syncResult.errorStrings().isEmpty();
    if (syncError) {
        qCWarning(lcFolder) << "SyncEngine finished with ERROR";
    } else {
        qCInfo(lcFolder) << "SyncEngine finished without problem.";
    }
    _fileLog->finish();
    showSyncResultPopup();

    auto anotherSyncNeeded = _engine->isAnotherSyncNeeded();

    auto syncStatus = SyncResult::Status::Undefined;

    if (syncError) {
        syncStatus = SyncResult::Error;
    } else if (_syncResult.foundFilesNotSynced()) {
        syncStatus = SyncResult::Problem;
    } else if (_definition.paused) {
        // Maybe the sync was terminated because the user paused the folder
        syncStatus = SyncResult::Paused;
    } else {
        syncStatus = SyncResult::Success;
    }

    // Count the number of syncs that have failed in a row.
    if (syncStatus == SyncResult::Success || syncStatus == SyncResult::Problem) {
        _consecutiveFailingSyncs = 0;
    } else {
        _consecutiveFailingSyncs++;
        qCInfo(lcFolder) << "the last" << _consecutiveFailingSyncs << "syncs failed";
    }

    if (syncStatus == SyncResult::Success && success) {
        // Clear the white list as all the folders that should be on that list are sync-ed
        journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {});
    }

    if ((syncStatus == SyncResult::Success || syncStatus == SyncResult::Problem) && success) {
        if (_engine->lastLocalDiscoveryStyle() == LocalDiscoveryStyle::FilesystemOnly) {
            _timeSinceLastFullLocalDiscovery.start();
        }
    }

    if (syncStatus != SyncResult::Undefined) {
        setSyncState(syncStatus);
    }

    // syncStateChange from setSyncState needs to be emitted first
    QTimer::singleShot(0, this, [this] { Q_EMIT syncFinished(_syncResult); });

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
        QTimer::singleShot(SyncEngine::minimumFileAgeForUpload, this, [this] { FolderMan::instance()->scheduler()->enqueueFolder(this); });
    }
}

// a item is completed: count the errors and forward to the ProgressDispatcher
void Folder::slotItemCompleted(const SyncFileItemPtr &item)
{
    if (item->_status == SyncFileItem::Success
        && (item->_instruction & (CSYNC_INSTRUCTION_NONE | CSYNC_INSTRUCTION_UPDATE_METADATA))) {
        // We only care about the updates that deserve to be shown in the UI
        return;
    }

    _syncResult.processCompletedItem(item);

    _fileLog->logItem(*item);
    emit ProgressDispatcher::instance()->itemCompleted(this, item);
}

void Folder::slotNewBigFolderDiscovered(const QString &newF, bool isExternal)
{
    auto newFolder = newF;
    if (!newFolder.endsWith(QLatin1Char('/'))) {
        newFolder += QLatin1Char('/');
    }
    auto journal = journalDb();

    // Add the entry to the blacklist if it is neither in the blacklist or whitelist already
    bool ok1, ok2;
    auto blacklist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok1);
    auto whitelist = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok2);
    if (ok1 && ok2 && !blacklist.contains(newFolder) && !whitelist.contains(newFolder)) {
        blacklist.insert(newFolder);
        journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blacklist);
    }

    // And add the entry to the undecided list and signal the UI
    auto undecidedList = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok1);
    if (ok1) {
        if (!undecidedList.contains(newFolder)) {
            undecidedList.insert(newFolder);
            journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, undecidedList);
            emit newBigFolderDiscovered(newFolder);
        }
        QString message = !isExternal ? (tr("A new folder larger than %1 MB has been added: %2.\n")
                                                .arg(ConfigFile().newBigFolderSizeLimit().second)
                                                .arg(newF))
                                      : (tr("A folder from an external storage has been added.\n"));
        message += tr("Please go in the settings to select it if you wish to download it.");

        ocApp()->gui()->slotShowOptionalTrayMessage(Theme::instance()->appNameGUI(), message);
    }
}

void Folder::slotLogPropagationStart()
{
    _fileLog->logLap(QStringLiteral("Propagation starts"));
}

void Folder::slotNextSyncFullLocalDiscovery()
{
    _timeSinceLastFullLocalDiscovery.invalidate();
}

void Folder::schedulePathForLocalDiscovery(const QString &relativePath)
{
    _localDiscoveryTracker->addTouchedPath(relativePath);
}

void Folder::slotFolderConflicts(Folder *folder, const QStringList &conflictPaths)
{
    if (folder != this)
        return;
    auto &r = _syncResult;

    // If the number of conflicts is too low, adjust it upwards
    if (conflictPaths.size() > r.numNewConflictItems() + r.numOldConflictItems())
        r.setNumOldConflictItems(conflictPaths.size() - r.numNewConflictItems());
}

void Folder::warnOnNewExcludedItem(const SyncJournalFileRecord &record, QStringView path)
{
    // Never warn for items in the database
    if (record.isValid())
        return;

    // Don't warn for items that no longer exist.
    // Note: This assumes we're getting file watcher notifications
    // for folders only on creation and deletion - if we got a notification
    // on content change that would create spurious warnings.
    QFileInfo fi(_canonicalLocalPath + path);
    if (!fi.exists())
        return;

    bool ok = false;
    auto blacklist = _journal.getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    if (!ok)
        return;
    if (!blacklist.contains(path + QLatin1Char('/')))
        return;

    const auto message = fi.isDir()
        ? tr("The folder %1 was created but was excluded from synchronization previously. "
             "Data inside it will not be synchronized.")
              .arg(fi.filePath())
        : tr("The file %1 was created but was excluded from synchronization previously. "
             "It will not be synchronized.")
              .arg(fi.filePath());

    ocApp()->gui()->slotShowOptionalTrayMessage(Theme::instance()->appNameGUI(), message);
}

void Folder::slotWatcherUnreliable(const QString &message)
{
    qCWarning(lcFolder) << "Folder watcher for" << path() << "became unreliable:" << message;

    QMessageBox *msgBox = new QMessageBox(QMessageBox::Information, Theme::instance()->appNameGUI(),
        tr("Changes in synchronized folders could not be tracked reliably.\n"
           "\n"
           "This means that the synchronization client might not upload local changes "
           "immediately and will instead only scan for local changes and upload them "
           "occasionally (every two hours by default).\n"
           "\n"
           "%1")
            .arg(message),
        {}, ocApp()->gui()->settingsDialog());

    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->open();
    ocApp()->gui()->raiseDialog(msgBox);
}

void Folder::registerFolderWatcher()
{
    if (!_folderWatcher.isNull()) {
        return;
    }

    _folderWatcher.reset(new FolderWatcher(this));
    connect(_folderWatcher.data(), &FolderWatcher::pathChanged, this,
        [this](const QSet<QString> &paths) { slotWatchedPathsChanged(paths, Folder::ChangeReason::Other); });
    connect(_folderWatcher.data(), &FolderWatcher::lostChanges,
        this, &Folder::slotNextSyncFullLocalDiscovery);
    connect(_folderWatcher.data(), &FolderWatcher::becameUnreliable,
        this, &Folder::slotWatcherUnreliable);
    _folderWatcher->init(path());
    _folderWatcher->startNotificatonTest(path() + QLatin1String(".owncloudsync.log"));
}

bool Folder::virtualFilesEnabled() const
{
    return _definition.virtualFilesMode != Vfs::Off && !isVfsOnOffSwitchPending();
}

void Folder::slotAboutToRemoveAllFiles(SyncFileItem::Direction direction)
{
    const QString msg = [direction] {
        if (direction == SyncFileItem::Down) {
            return tr("All files in the sync folder '%1' folder were deleted on the server.\n"
                      "These deletes will be synchronized to your local sync folder, making such files "
                      "unavailable unless you have a right to restore. \n"
                      "If you decide to keep the files, they will be re-synced with the server if you have rights to do so.\n"
                      "If you decide to delete the files, they will be unavailable to you, unless you are the owner.");
        } else {
            return tr("All the files in your local sync folder '%1' were deleted. These deletes will be "
                      "synchronized with your server, making such files unavailable unless restored.\n"
                      "Are you sure you want to sync those actions with the server?\n"
                      "If this was an accident and you decide to keep your files, they will be re-synced from the server.");
        }
    }();
    auto msgBox = new QMessageBox(QMessageBox::Warning, tr("Remove All Files?"),
        msg.arg(shortGuiLocalPath()), QMessageBox::NoButton, ocApp()->gui()->settingsDialog());
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->addButton(tr("Remove all files"), QMessageBox::DestructiveRole);
    QPushButton *keepBtn = msgBox->addButton(tr("Keep files"), QMessageBox::AcceptRole);
    msgBox->setDefaultButton(keepBtn);
    setSyncPaused(true);
    connect(msgBox, &QMessageBox::finished, this, [msgBox, keepBtn, this] {
        if (msgBox->clickedButton() == keepBtn) {
            // reset the db upload all local files or download all remote files
            FileSystem::setFolderMinimumPermissions(path());
            // will remove placeholders in the next sync
            SyncEngine::wipeVirtualFiles(path(), _journal, *_vfs);
            journalDb()->clearFileTable();
        }
        // if all local files where placeholders, they might be gone after the next sync
        // therefor we need to allow removal off all files in the next sync even if we selected keep
        _allowRemoveAllOnce = true;
        // the only way we end up in here is that the folder was not paused
        setSyncPaused(false);
        FolderMan::instance()->scheduler()->enqueueFolder(this);
    });
    connect(this, &Folder::destroyed, msgBox, &QMessageBox::deleteLater);
    msgBox->open();
    ownCloudGui::raiseDialog(msgBox);
}

FolderDefinition::FolderDefinition(const QByteArray &id, const QUrl &davUrl, const QString &spaceId, const QString &displayName)
    : _webDavUrl(davUrl)
    , _spaceId(spaceId)
    , _id(id)
    , _displayName(displayName)
{
}

void FolderDefinition::setPriority(uint32_t newPriority)
{
    _priority = newPriority;
}

uint32_t FolderDefinition::priority() const
{
    return _priority;
}

void FolderDefinition::save(QSettings &settings, const FolderDefinition &folder)
{
    settings.setValue(QStringLiteral("localPath"), folder.localPath());
    settings.setValue(QStringLiteral("journalPath"), folder.journalPath);
    settings.setValue(QStringLiteral("targetPath"), folder.targetPath());
    if (!folder.spaceId().isEmpty()) {
        settings.setValue(spaceIdC(), folder.spaceId());
    }
    settings.setValue(davUrlC(), folder.webDavUrl());
    settings.setValue(displayNameC(), folder.displayName());
    settings.setValue(QStringLiteral("paused"), folder.paused);
    settings.setValue(QStringLiteral("ignoreHiddenFiles"), folder.ignoreHiddenFiles);
    settings.setValue(deployedC(), folder.isDeployed());
    settings.setValue(priorityC(), folder.priority());

    settings.setValue(QStringLiteral("virtualFilesMode"), Utility::enumToString(folder.virtualFilesMode));

    // Prevent loading of profiles in old clients
    settings.setValue(versionC(), ConfigFile::UnusedLegacySettingsVersionNumber);
}

FolderDefinition FolderDefinition::load(QSettings &settings, const QByteArray &id)
{
    FolderDefinition folder{id, settings.value(davUrlC()).toUrl(), settings.value(spaceIdC()).toString(), settings.value(displayNameC()).toString()};
    folder.setLocalPath(settings.value(QStringLiteral("localPath")).toString());
    folder.journalPath = settings.value(QStringLiteral("journalPath")).toString();
    folder.setTargetPath(settings.value(QStringLiteral("targetPath")).toString());
    folder.paused = settings.value(QStringLiteral("paused")).toBool();
    folder.ignoreHiddenFiles = settings.value(QStringLiteral("ignoreHiddenFiles"), QVariant(true)).toBool();
    folder._deployed = settings.value(deployedC(), false).toBool();
    folder._priority = settings.value(priorityC(), 0).toInt();

    folder.virtualFilesMode = Vfs::Off;
    QString vfsModeString = settings.value(QStringLiteral("virtualFilesMode")).toString();
    if (!vfsModeString.isEmpty()) {
        if (auto mode = Vfs::modeFromString(vfsModeString)) {
            folder.virtualFilesMode = *mode;
        } else {
            qCWarning(lcFolder) << "Unknown virtualFilesMode:" << vfsModeString << "assuming 'off'";
        }
    } else {
        if (settings.value(QStringLiteral("usePlaceholders")).toBool()) {
            folder.virtualFilesMode = Vfs::WithSuffix;
            folder.upgradeVfsMode = true; // maybe winvfs is available?
        }
    }
    return folder;
}

void FolderDefinition::setLocalPath(const QString &path)
{
    _localPath = QDir::fromNativeSeparators(path);
    if (!_localPath.endsWith(QLatin1Char('/'))) {
        _localPath.append(QLatin1Char('/'));
    }
}

void FolderDefinition::setTargetPath(const QString &path)
{
    _targetPath = Utility::stripTrailingSlash(path);
    // Doing this second ensures the empty string or "/" come
    // out as "/".
    if (!_targetPath.startsWith(QLatin1Char('/'))) {
        _targetPath.prepend(QLatin1Char('/'));
    }
}

QString FolderDefinition::absoluteJournalPath() const
{
    return QDir(localPath()).filePath(journalPath);
}

const QByteArray &FolderDefinition::id() const
{
    return _id;
}

QString FolderDefinition::displayName() const
{
    if (_displayName.isEmpty()) {
        if (targetPath().length() > 0 && targetPath() != QLatin1String("/")) {
            QString a = QFileInfo(targetPath()).fileName();
            if (a.startsWith(QLatin1Char('/'))) {
                a = a.remove(0, 1);
            }
            return a;
        } else {
            return Theme::instance()->appNameGUI();
        }
    }
    return _displayName;
}

bool Folder::groupInSidebar() const
{
    if (_accountState->account()->hasDefaultSyncRoot()) {
        // QFileInfo is horrible and "/foo/" is treated different to "/foo"
        const QString parentDir = QFileInfo(Utility::stripTrailingSlash(path())).dir().path();
        Q_ASSERT(QFileInfo(parentDir) != QFileInfo(path()));
        // If parentDir == home, we would add a the home dir to the side bar.
        return QFileInfo(parentDir) != QFileInfo(QDir::homePath()) && FileSystem::isChildPathOf(parentDir, _accountState->account()->defaultSyncRoot());
    }
    return false;
}

QString Folder::spaceId() const
{
    return _definition.spaceId();
}

bool FolderDefinition::isDeployed() const
{
    return _deployed;
}

QUrl FolderDefinition::webDavUrl() const
{
    Q_ASSERT(_webDavUrl.isValid());
    return _webDavUrl;
}

QString FolderDefinition::targetPath() const
{
    return _targetPath;
}

QString FolderDefinition::localPath() const
{
    return _localPath;
}

QString FolderDefinition::spaceId() const
{
    // we might call the function to check for the id
    // anyhow one of the conditions needs to be true
    Q_ASSERT(_webDavUrl.isValid() || !_spaceId.isEmpty());
    return _spaceId;
}
} // namespace OCC
