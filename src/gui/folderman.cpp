/*
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

#include "folderman.h"
#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "application.h"
#include "common/asserts.h"
#include "configfile.h"
#include "filesystem.h"
#include "folder.h"
#include "libsync/syncengine.h"
#include "lockwatcher.h"
#include "scheduling/syncscheduler.h"
#include "socketapi/socketapi.h"
#include "syncresult.h"
#include "theme.h"

#ifdef Q_OS_WIN
#include "common/utility_win.h"
#endif

#include <QMessageBox>
#include <QMutableSetIterator>
#include <QNetworkProxy>
#include <QtCore>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
int numberOfSyncJournals(const QString &path)
{
    return QDir(path).entryList({ QStringLiteral(".sync_*.db"), QStringLiteral("._sync_*.db") }, QDir::Hidden | QDir::Files).size();
}

QString makeLegacyDbName(const OCC::FolderDefinition &def, const OCC::AccountPtr &account)
{
    // ensure https://demo.owncloud.org/ matches https://demo.owncloud.org
    // the empty path was the legacy formating before 2.9
    auto legacyUrl = account->url();
    if (legacyUrl.path() == QLatin1String("/")) {
        legacyUrl.setPath(QString());
    }
    const QString key = QStringLiteral("%1@%2:%3").arg(account->credentials()->user(), legacyUrl.toString(), def.targetPath());
    return OCC::SyncJournalDb::makeDbName(def.localPath(), QString::fromUtf8(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).left(6).toHex()));
}
}

namespace OCC {
Q_LOGGING_CATEGORY(lcFolderMan, "gui.folder.manager", QtInfoMsg)

void TrayOverallStatusResult::addResult(Folder *f)
{
    _overallStatus._numNewConflictItems += f->syncResult()._numNewConflictItems;
    _overallStatus._numErrorItems += f->syncResult()._numErrorItems;
    _overallStatus._numBlacklistErrors += f->syncResult()._numBlacklistErrors;

    auto time = f->lastSyncTime();
    if (time > lastSyncDone) {
        lastSyncDone = time;
    }

    auto status = f->syncPaused() ? SyncResult::Paused : f->syncResult().status();
    if (status == SyncResult::Undefined) {
        status = SyncResult::Problem;
    }
    if (status > _overallStatus.status()) {
        _overallStatus.setStatus(status);
    }
}

const SyncResult &TrayOverallStatusResult::overallStatus() const
{
    return _overallStatus;
}

FolderMan *FolderMan::_instance = nullptr;

FolderMan::FolderMan()
    : _lockWatcher(new LockWatcher)
    , _scheduler(new SyncScheduler(this))
    , _appRestartRequired(false)
    , _socketApi(new SocketApi)
{
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &FolderMan::slotRemoveFoldersForAccount);

    connect(_lockWatcher.data(), &LockWatcher::fileUnlocked, this, [this](const QString &path, FileSystem::LockMode) {
        if (Folder *f = folderForPath(path)) {
            // Treat this equivalently to the file being reported by the file watcher
            f->slotWatchedPathsChanged({path}, Folder::ChangeReason::UnLock);
        }
    });
}

FolderMan *FolderMan::instance()
{
    Q_ASSERT(_instance);
    return _instance;
}

FolderMan::~FolderMan()
{
    unloadAndDeleteAllFolders();
    qDeleteAll(_folders);
    _instance = nullptr;
}

const QVector<Folder *> &FolderMan::folders() const
{
    return _folders;
}

void FolderMan::unloadFolder(Folder *f)
{
    Q_ASSERT(f);

    _folders.removeAll(f);
    _socketApi->slotUnregisterPath(f);


    if (!f->hasSetupError()) {
        disconnect(f, nullptr, _socketApi.get(), nullptr);
        disconnect(f, nullptr, this, nullptr);
        disconnect(f, nullptr, &f->syncEngine().syncFileStatusTracker(), nullptr);
        disconnect(&f->syncEngine(), nullptr, f, nullptr);
        disconnect(
            &f->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged, _socketApi.get(), &SocketApi::broadcastStatusPushMessage);
    }
}

void FolderMan::unloadAndDeleteAllFolders()
{
    // clear the list of existing folders.
    const auto folders = std::move(_folders);
    for (auto *folder : folders) {
        folder->saveToSettings();
        _socketApi->slotUnregisterPath(folder);
        folder->deleteLater();
    }
    emit folderListChanged();
}

void FolderMan::registerFolderWithSocketApi(Folder *folder)
{
    if (!folder)
        return;
    if (!QDir(folder->path()).exists())
        return;

    // register the folder with the socket API
    if (folder->canSync())
        _socketApi->slotRegisterPath(folder);
}

std::optional<qsizetype> FolderMan::setupFolders()
{
    unloadAndDeleteAllFolders();

    auto settings = ConfigFile::settingsWithGroup(QStringLiteral("Accounts"));
    const auto &accountsWithSettings = settings->childGroups();

    qCInfo(lcFolderMan) << "Setup folders from settings file";

    for (const auto &account : AccountManager::instance()->accounts()) {
        const auto id = account->account()->id();
        if (!accountsWithSettings.contains(id)) {
            continue;
        }

        settings->beginGroup(id); // Process settings for this account.

        auto process = [&](const QString &groupName) -> bool {
            settings->beginGroup(groupName);
            bool success = setupFoldersHelper(*settings, account);
            settings->endGroup();
            return success;
        };

        if (!process(QStringLiteral("Folders"))) {
            return {};
        }

        // removed in 5.0
        {
            if (!process(QStringLiteral("FoldersWithPlaceholders"))) {
                return {};
            }

            // We don't save to `Multifolders` anymore, but for backwards compatibility we will just
            // read it like it is a `Folders` entry.
            if (!process(QStringLiteral("Multifolders"))) {
                return {};
            }
        }

        settings->endGroup(); // Finished processing this account.
    }

    emit folderListChanged();

    return _folders.size();
}

bool FolderMan::setupFoldersHelper(QSettings &settings, AccountStatePtr account)
{
    const auto &childGroups = settings.childGroups();
    for (const auto &folderAlias : childGroups) {
        settings.beginGroup(folderAlias);
        FolderDefinition folderDefinition = FolderDefinition::load(settings, folderAlias.toUtf8());
        const auto defaultJournalPath = [&account, folderDefinition] {
            // if we would have booth the 2.9.0 file name and the lagacy file
            // with the md5 infix we prefer the 2.9.0 version
            const QDir info(folderDefinition.localPath());
            const QString defaultPath = SyncJournalDb::makeDbName(folderDefinition.localPath());
            if (info.exists(defaultPath)) {
                return defaultPath;
            }
            // 2.6
            QString legacyPath = makeLegacyDbName(folderDefinition, account->account());
            if (info.exists(legacyPath)) {
                return legacyPath;
            }
            // pre 2.6
            legacyPath.replace(QLatin1String(".sync_"), QLatin1String("._sync_"));
            if (info.exists(legacyPath)) {
                return legacyPath;
            }
            return defaultPath;
        }();

        // migration: 2.10 did not specify a webdav url
        if (folderDefinition._webDavUrl.isEmpty()) {
            folderDefinition._webDavUrl = account->account()->davUrl();
        }

        // Migration: Old settings don't have journalPath
        if (folderDefinition.journalPath.isEmpty()) {
            folderDefinition.journalPath = defaultJournalPath;
        }

        // Migration: ._ files sometimes can't be created.
        // So if the configured journalPath has a dot-underscore ("._sync_*.db")
        // but the current default doesn't have the underscore, switch to the
        // new default if no db exists yet.
        if (folderDefinition.journalPath.startsWith(QLatin1String("._sync_"))
            && defaultJournalPath.startsWith(QLatin1String(".sync_"))
            && !QFile::exists(folderDefinition.absoluteJournalPath())) {
            folderDefinition.journalPath = defaultJournalPath;
        }

        // Migration: If an old .csync_journal.db is found, move it to the new name.
        SyncJournalDb::maybeMigrateDb(folderDefinition.localPath(), folderDefinition.absoluteJournalPath());
        if (SyncJournalDb::dbIsTooNewForClient(folderDefinition.absoluteJournalPath())) {
            return false;
        }

        auto vfs = VfsPluginManager::instance().createVfsFromPlugin(folderDefinition.virtualFilesMode);
        if (!vfs) {
            // TODO: Must do better error handling
            qFatal("Could not load plugin");
        }

        if (Folder *f = addFolderInternal(std::move(folderDefinition), account, std::move(vfs))) {
            // save possible changes from the migration
            f->saveToSettings();
            emit folderSyncStateChange(f);
        }
        settings.endGroup();
    }

    return true;
}

bool FolderMan::ensureJournalGone(const QString &journalDbFile)
{
    // remove the old journal file
    while (QFile::exists(journalDbFile) && !QFile::remove(journalDbFile)) {
        qCWarning(lcFolderMan) << "Could not remove old db file at" << journalDbFile;
        int ret = QMessageBox::warning(nullptr, tr("Could not reset folder state"),
            tr("An old sync journal '%1' was found, "
               "but could not be removed. Please make sure "
               "that no application is currently using it.")
                .arg(QDir::fromNativeSeparators(QDir::cleanPath(journalDbFile))),
            QMessageBox::Retry | QMessageBox::Abort);
        if (ret == QMessageBox::Abort) {
            return false;
        }
    }
    return true;
}

SocketApi *FolderMan::socketApi()
{
    return _socketApi.get();
}

void FolderMan::slotFolderSyncPaused(Folder *f, bool paused)
{
    if (!f) {
        qCCritical(lcFolderMan) << "slotFolderSyncPaused called with empty folder";
        return;
    }

    if (!paused) {
        _disabledFolders.remove(f);
        scheduler()->enqueueFolder(f);
    } else {
        _disabledFolders.insert(f);
    }
}

void FolderMan::slotFolderCanSyncChanged()
{
    Folder *f = qobject_cast<Folder *>(sender());
    OC_ASSERT(f);
    if (f->canSync()) {
        _socketApi->slotRegisterPath(f);
    } else {
        _socketApi->slotUnregisterPath(f);
    }
}

Folder *FolderMan::folder(const QByteArray &id)
{
    if (!id.isEmpty()) {
        auto f = std::find_if(_folders.cbegin(), _folders.cend(), [id](auto f) {
            return f->id() == id;
        });
        if (f != _folders.cend()) {
            return *f;
        }
    }
    return nullptr;
}

void FolderMan::scheduleAllFolders()
{
    for (auto *f : qAsConst(_folders)) {
        if (f && f->canSync()) {
            scheduler()->enqueueFolder(f);
        }
    }
}

void FolderMan::slotScheduleAppRestart()
{
    _appRestartRequired = true;
    qCInfo(lcFolderMan) << "Application restart requested!";
}

void FolderMan::slotSyncOnceFileUnlocks(const QString &path, FileSystem::LockMode mode)
{
    _lockWatcher->addFile(path, mode);
}

void FolderMan::slotIsConnectedChanged()
{
    AccountStatePtr accountState(qobject_cast<AccountState *>(sender()));
    if (!accountState) {
        return;
    }
    QString accountName = accountState->account()->displayName();

    if (accountState->isConnected()) {
        qCInfo(lcFolderMan) << "Account" << accountName << "connected, scheduling its folders";

        for (auto *f : qAsConst(_folders)) {
            if (f
                && f->canSync()
                && f->accountState() == accountState) {
                scheduler()->enqueueFolder(f);
            }
        }
    } else {
        qCInfo(lcFolderMan) << "Account" << accountName << "disconnected or paused, "
                                                           "terminating or descheduling sync folders";

        for (auto *f : qAsConst(_folders)) {
            if (f
                && f->isSyncRunning()
                && f->accountState() == accountState) {
                f->slotTerminateSync();
            }
        }
    }
}

// only enable or disable foldermans will schedule and do syncs.
// this is not the same as Pause and Resume of folders.
void FolderMan::setSyncEnabled(bool enabled)
{
    if (enabled) {
        // We have things in our queue that were waiting for the connection to come back on.
        scheduler()->start();
    } else {
        scheduler()->stop();
    }
    qCInfo(lcFolderMan) << Q_FUNC_INFO << enabled;
    // force a redraw in case the network connect status changed
    Q_EMIT folderSyncStateChange(nullptr);
}

void FolderMan::slotRemoveFoldersForAccount(const AccountStatePtr &accountState)
{
    QList<Folder *> foldersToRemove;
    // reserve a magic number
    foldersToRemove.reserve(16);
    for (auto *folder : qAsConst(_folders)) {
        if (folder->accountState() == accountState) {
            foldersToRemove.append(folder);
        }
    }
    for (const auto &f : foldersToRemove) {
        removeFolder(f);
    }
}

void FolderMan::slotServerVersionChanged(Account *account)
{
    // Pause folders if the server version is unsupported
    if (account->serverSupportLevel() == Account::ServerSupportLevel::Unsupported) {
        qCWarning(lcFolderMan) << "The server version is unsupported:" << account->capabilities().status().versionString()
                               << "pausing all folders on the account";

        for (auto &f : qAsConst(_folders)) {
            if (f->accountState()->account().data() == account) {
                f->setSyncPaused(true);
            }
        }
    }
}

bool FolderMan::isAnySyncRunning() const
{
    if (_scheduler->hasCurrentRunningSyncRunning()) {
        return true;
    }

    for (auto f : _folders) {
        if (f->isSyncRunning())
            return true;
    }
    return false;
}

void FolderMan::slotFolderSyncStarted()
{
    auto f = qobject_cast<Folder *>(sender());
    OC_ASSERT(f);
    if (!f)
        return;

    qCInfo(lcFolderMan) << ">========== Sync started for folder ["
                        << f->shortGuiLocalPath()
                        << "] of account ["
                        << f->accountState()->account()->displayName()
                        << "] with remote ["
                        << f->remoteUrl().toDisplayString()
                        << "]";
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  * This delay is particularly useful to avoid late file change notifications
  * (that we caused ourselves by syncing) from triggering another spurious sync.
  */
void FolderMan::slotFolderSyncFinished(const SyncResult &)
{
    auto f = qobject_cast<Folder *>(sender());
    OC_ASSERT(f);
    if (!f)
        return;

    qCInfo(lcFolderMan) << "<========== Sync finished for folder [" << f->shortGuiLocalPath() << "] of account [" << f->accountState()->account()->displayName()
                        << "] with remote [" << f->remoteUrl().toDisplayString() << "]";
}

Folder *FolderMan::addFolder(const AccountStatePtr &accountState, const FolderDefinition &folderDefinition)
{
    // Choose a db filename
    auto definition = folderDefinition;
    definition.journalPath = SyncJournalDb::makeDbName(folderDefinition.localPath());

    if (!ensureJournalGone(definition.absoluteJournalPath())) {
        return nullptr;
    }

    auto vfs = VfsPluginManager::instance().createVfsFromPlugin(folderDefinition.virtualFilesMode);
    if (!vfs) {
        qCWarning(lcFolderMan) << "Could not load plugin for mode" << folderDefinition.virtualFilesMode;
        return nullptr;
    }

    auto folder = addFolderInternal(definition, accountState, std::move(vfs));

    if (folder) {
        folder->saveToSettings();
        emit folderSyncStateChange(folder);
        emit folderListChanged();
    }

    return folder;
}

Folder *FolderMan::addFolderInternal(
    FolderDefinition folderDefinition,
    const AccountStatePtr &accountState,
    std::unique_ptr<Vfs> vfs)
{
    // ensure we don't add multiple legacy folders with the same id
    if (!OC_ENSURE(!folderDefinition.id().isEmpty() && !folder(folderDefinition.id()))) {
        folderDefinition._id = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    }

    auto folder = new Folder(folderDefinition, accountState, std::move(vfs), this);

    qCInfo(lcFolderMan) << "Adding folder to Folder Map " << folder << folder->path();
    _folders.push_back(folder);
    if (folder->syncPaused()) {
        _disabledFolders.insert(folder);
    }

    // See matching disconnects in unloadFolder().
    if (!folder->hasSetupError()) {
        connect(folder, &Folder::syncStateChange, _socketApi.get(), [folder, this] { _socketApi->slotUpdateFolderView(folder); });
        connect(folder, &Folder::syncStarted, this, &FolderMan::slotFolderSyncStarted);
        connect(folder, &Folder::syncFinished, this, &FolderMan::slotFolderSyncFinished);
        connect(folder, &Folder::syncStateChange, this, [folder, this] { Q_EMIT folderSyncStateChange(folder); });
        connect(folder, &Folder::syncPausedChanged, this, &FolderMan::slotFolderSyncPaused);
        connect(folder, &Folder::canSyncChanged, this, &FolderMan::slotFolderCanSyncChanged);
        connect(
            &folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged, _socketApi.get(), &SocketApi::broadcastStatusPushMessage);
        connect(folder, &Folder::watchedFileChangedExternally, &folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::slotPathTouched);
        registerFolderWithSocketApi(folder);
    }
    return folder;
}

Folder *FolderMan::folderForPath(const QString &path, QString *relativePath)
{
    QString absolutePath = QDir::cleanPath(path) + QLatin1Char('/');

    for (auto *folder : qAsConst(_folders)) {
        const QString folderPath = folder->cleanPath() + QLatin1Char('/');

        if (absolutePath.startsWith(folderPath, (Utility::isWindows() || Utility::isMac()) ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
            if (relativePath) {
                *relativePath = absolutePath.mid(folderPath.length());
                relativePath->chop(1); // we added a '/' above
            }
            return folder;
        }
    }

    if (relativePath)
        relativePath->clear();
    return nullptr;
}

QStringList FolderMan::findFileInLocalFolders(const QString &relPath, const AccountPtr acc)
{
    QStringList re;

    // We'll be comparing against Folder::remotePath which always starts with /
    QString serverPath = relPath;
    if (!serverPath.startsWith(QLatin1Char('/')))
        serverPath.prepend(QLatin1Char('/'));

    for (auto *folder : qAsConst(_folders)) {
        if (acc != nullptr && folder->accountState()->account() != acc) {
            continue;
        }
        if (!serverPath.startsWith(folder->remotePath()))
            continue;

        QString path = folder->cleanPath() + QLatin1Char('/');
        path += serverPath.mid(folder->remotePathTrailingSlash().length());
        if (QFile::exists(path)) {
            re.append(path);
        }
    }
    return re;
}

void FolderMan::removeFolder(Folder *f)
{
    if (!OC_ENSURE(f)) {
        return;
    }

    qCInfo(lcFolderMan) << "Removing " << f->path();

    const bool currentlyRunning = f->isSyncRunning();
    if (currentlyRunning) {
        // abort the sync now
        f->slotTerminateSync();
    }

    f->setSyncPaused(true);
    f->wipeForRemoval();

    // remove the folder configuration
    f->removeFromSettings();

    unloadFolder(f);
    f->deleteLater();

    Q_EMIT folderRemoved(f);
    emit folderListChanged();
}

QString FolderMan::getBackupName(QString fullPathName) const
{
    if (fullPathName.endsWith(QLatin1String("/")))
        fullPathName.chop(1);

    if (fullPathName.isEmpty())
        return QString();

    QString newName = fullPathName + tr(" (backup)");
    QFileInfo fi(newName);
    int cnt = 2;
    do {
        if (fi.exists()) {
            newName = fullPathName + tr(" (backup %1)").arg(cnt++);
            fi.setFile(newName);
        }
    } while (fi.exists());

    return newName;
}

void FolderMan::setDirtyProxy()
{
    for (auto *f : qAsConst(_folders)) {
        if (f) {
            if (f->accountState() && f->accountState()->account()
                && f->accountState()->account()->accessManager()) {
                // Need to do this so we do not use the old determined system proxy
                f->accountState()->account()->accessManager()->setProxy(
                    QNetworkProxy(QNetworkProxy::DefaultProxy));
            }
        }
    }
}

void FolderMan::setDirtyNetworkLimits()
{
    for (auto *f : qAsConst(_folders)) {
        // set only in busy folders. Otherwise they read the config anyway.
        if (f && f->isSyncRunning()) {
            f->setDirtyNetworkLimits();
        }
    }
}

TrayOverallStatusResult FolderMan::trayOverallStatus(const QVector<Folder *> &folders)
{
    TrayOverallStatusResult result;

    // if one of them has an error -> show error
    // if one is paused, but others ok, show ok
    //
    for (auto *folder : folders) {
        result.addResult(folder);
    }
    return result;
}

QString FolderMan::trayTooltipStatusString(
    const SyncResult &result, bool paused)
{
    QString folderMessage;
    switch (result.status()) {
    case SyncResult::Success:
        [[fallthrough]];
    case SyncResult::Problem:
        if (result.hasUnresolvedConflicts()) {
            folderMessage = tr("Sync was successful, unresolved conflicts.");
            break;
        }
        [[fallthrough]];
    default:
        return Utility::enumToDisplayName(result.status());
    }
    if (paused) {
        // sync is disabled.
        folderMessage = tr("%1 (Sync is paused)").arg(folderMessage);
    }
    return folderMessage;
}

// QFileInfo::canonicalPath returns an empty string if the file does not exist.
// This function also works with files that does not exist and resolve the symlinks in the
// parent directories.
static QString canonicalPath(const QString &path)
{
    QFileInfo selFile(path);
    if (!selFile.exists()) {
        const auto parentPath = selFile.dir().path();

        // It's possible for the parentPath to match the path
        // (possibly we've arrived at a non-existant drive root on Windows)
        // and recursing would be fatal.
        if (parentPath == path) {
            return path;
        }

        return canonicalPath(parentPath) + QLatin1Char('/') + selFile.fileName();
    }
    return selFile.canonicalFilePath();
}

QString FolderMan::checkPathValidityRecursive(const QString &path)
{
    if (path.isEmpty()) {
        return FolderMan::tr("No valid folder selected!");
    }

#ifdef Q_OS_WIN
    Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif

    auto pathLenghtCheck = Folder::checkPathLength(path);
    if (!pathLenghtCheck) {
        return pathLenghtCheck.error();
    }

    const QFileInfo selFile(path);
    if (numberOfSyncJournals(selFile.filePath()) != 0) {
        return FolderMan::tr("The folder %1 is used in a folder sync connection!").arg(QDir::toNativeSeparators(selFile.filePath()));
    }

    if (!selFile.exists()) {
        const QString parentPath = selFile.path();
        if (parentPath != path)
            return checkPathValidityRecursive(parentPath);
        return FolderMan::tr("The selected path does not exist!");
    }

    if (!selFile.isDir()) {
        return FolderMan::tr("The selected path is not a folder!");
    }

    if (!selFile.isWritable()) {
        return FolderMan::tr("You have no permission to write to the selected folder!");
    }
    return QString();
}

QString FolderMan::checkPathValidityForNewFolder(const QString &path) const
{
    // check if the local directory isn't used yet in another ownCloud sync
    const auto cs = Utility::fsCaseSensitivity();

    const QString userDir = QDir::cleanPath(canonicalPath(path)) + QLatin1Char('/');
    for (auto f : _folders) {
        const QString folderDir = QDir::cleanPath(canonicalPath(f->path())) + QLatin1Char('/');

        if (QString::compare(folderDir, userDir, cs) == 0) {
            return tr("There is already a sync from the server to this local folder. "
                      "Please pick another local folder!");
        }
        if (FileSystem::isChildPathOf(folderDir, userDir)) {
            return tr("The local folder %1 already contains a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(path));
        }

        if (FileSystem::isChildPathOf(userDir, folderDir)) {
            return tr("The local folder %1 is already contained in a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(path));
        }
    }

    const auto result = checkPathValidityRecursive(path);
    if (!result.isEmpty()) {
        return tr("%1 Please pick another one!").arg(result);
    }
    return {};
}

QString FolderMan::findGoodPathForNewSyncFolder(const QString &basePath, const QString &newFolder)
{
    // reserve extra characters to allow appending of a number
    const QString normalisedPath = FileSystem::createPortableFileName(basePath, FileSystem::pathEscape(newFolder), std::string_view(" (100)").size());

    // If the parent folder is a sync folder or contained in one, we can't
    // possibly find a valid sync folder inside it.
    // Example: Someone syncs their home directory. Then ~/foobar is not
    // going to be an acceptable sync folder path for any value of foobar.
    if (FolderMan::instance()->folderForPath(QFileInfo(normalisedPath).canonicalPath())) {
        // Any path with that parent is going to be unacceptable,
        // so just keep it as-is.
        return canonicalPath(normalisedPath);
    }
    // Count attempts and give up eventually
    {
        QString folder = normalisedPath;
        for (int attempt = 2; attempt <= 100; ++attempt) {
            if (!QFileInfo::exists(folder)
                && FolderMan::instance()->checkPathValidityForNewFolder(folder).isEmpty()) {
                return canonicalPath(folder);
            }
            folder = normalisedPath + QStringLiteral(" (%1)").arg(attempt);
        }
    }
    // we failed to find a non existing path
    return canonicalPath(normalisedPath);
}

bool FolderMan::ignoreHiddenFiles() const
{
    if (_folders.empty()) {
        return true;
    }
    return _folders.first()->ignoreHiddenFiles();
}

void FolderMan::setIgnoreHiddenFiles(bool ignore)
{
    // Note that the setting will revert to 'true' if all folders
    // are deleted...
    for (auto *folder : qAsConst(_folders)) {
        folder->setIgnoreHiddenFiles(ignore);
        folder->saveToSettings();
    }
}

void FolderMan::restartApplication()
{
    if (Utility::isLinux()) {
        // restart:
        qCInfo(lcFolderMan) << "Restarting application NOW, PID" << qApp->applicationPid() << "is ending.";
        qApp->quit();
        QStringList args = qApp->arguments();
        QString prg = args.takeFirst();

        QProcess::startDetached(prg, args);
    } else {
        qCDebug(lcFolderMan) << "On this platform we do not restart.";
    }
}

Result<void, QString> FolderMan::unsupportedConfiguration(const QString &path) const
{
    auto it = _unsupportedConfigurationError.find(path);
    if (it == _unsupportedConfigurationError.end()) {
        it = _unsupportedConfigurationError.insert(path, [&]() -> Result<void, QString> {
            if (numberOfSyncJournals(path) > 1) {
                const QString error = tr("Multiple accounts are sharing the folder %1.\n"
                                         "This configuration is know to lead to dataloss and is no longer supported.\n"
                                         "Please consider removing this folder from the account and adding it again.")
                                          .arg(path);
                if (Theme::instance()->warnOnMultipleDb()) {
                    qCWarning(lcFolderMan) << error;
                    return error;
                } else {
                    qCWarning(lcFolderMan) << error << "this error is not displayed to the user as this is a branded"
                                           << "client and the error itself might be a false positive caused by a previous broken migration";
                }
            }
            return {};
        }());
    }
    return *it;
}

bool FolderMan::checkVfsAvailability(const QString &path, Vfs::Mode mode) const
{
    return unsupportedConfiguration(path) && Vfs::checkAvailability(path, mode);
}

Folder *FolderMan::addFolderFromWizard(const AccountStatePtr &accountStatePtr, FolderDefinition &&folderDefinition, bool useVfs)
{
    if (!FolderMan::prepareFolder(folderDefinition.localPath())) {
        return {};
    }

    folderDefinition.ignoreHiddenFiles = ignoreHiddenFiles();

    if (useVfs) {
        folderDefinition.virtualFilesMode = VfsPluginManager::instance().bestAvailableVfsMode();
    }

    auto newFolder = addFolder(accountStatePtr, folderDefinition);

    if (newFolder) {
        // With spaces we only handle the main folder
        if (!newFolder->groupInSidebar()) {
            Utility::setupFavLink(folderDefinition.localPath());
        }
        if (!ConfigFile().newBigFolderSizeLimit().first) {
            // The user already accepted the selective sync dialog. everything is in the white list
            newFolder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {QStringLiteral("/")});
        }
        qCDebug(lcFolderMan) << "Local sync folder" << folderDefinition.localPath() << "successfully created!";
        newFolder->saveToSettings();
    } else {
        qCWarning(lcFolderMan) << "Failed to create local sync folder!";
    }
    return newFolder;
}

Folder *FolderMan::addFolderFromFolderWizardResult(const AccountStatePtr &accountStatePtr, const FolderWizard::Result &config)
{
    FolderDefinition definition = FolderDefinition::createNewFolderDefinition(config.davUrl, config.spaceId, config.displayName);
    definition.setLocalPath(config.localPath);
    definition.setTargetPath(config.remotePath);
    auto f = addFolderFromWizard(accountStatePtr, std::move(definition), config.useVirtualFiles);
    if (f) {
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, config.selectiveSyncBlackList);
        f->setPriority(config.priority);
        f->saveToSettings();
    }
    return f;
}

QString FolderMan::suggestSyncFolder(const QUrl &server, const QString &displayName)
{
    return FolderMan::instance()->findGoodPathForNewSyncFolder(QDir::homePath(), Theme::instance()->appName());
}

bool FolderMan::prepareFolder(const QString &folder)
{
    if (!QFileInfo::exists(folder)) {
        if (!OC_ENSURE(QDir().mkpath(folder))) {
            return false;
        }
        FileSystem::setFolderMinimumPermissions(folder);
        Folder::prepareFolder(folder);
    }
    return true;
}

std::unique_ptr<FolderMan> FolderMan::createInstance()
{
    OC_ASSERT(!_instance);
    _instance = new FolderMan();
    return std::unique_ptr<FolderMan>(_instance);
}

} // namespace OCC
