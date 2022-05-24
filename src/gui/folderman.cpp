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
#include "lockwatcher.h"
#include "ocwizard_deprecated.h"
#include "selectivesyncdialog.h"
#include "socketapi/socketapi.h"
#include "syncresult.h"
#include "theme.h"
#include <syncengine.h>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif

#include <QMessageBox>
#include <QtCore>
#include <QMutableSetIterator>
#include <QSet>
#include <QNetworkProxy>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
/*
 * [Accounts]
 * 0\version=1
 */
auto versionC()
{
    return QStringLiteral("version");
}

/*
 * Folders with a version > maxFoldersVersion will be removed
 * After the user was prompted for consent.
 */
constexpr int maxFoldersVersion = 1;

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

    const auto status = f->syncPaused() ? SyncResult::Paused : f->syncResult().status();
    switch (status) {
    case SyncResult::Paused:
        Q_FALLTHROUGH();
    case SyncResult::SyncAbortRequested:
        // Problem has a high enum value but real problems and errors
        // take precedence
        if (_overallStatus.status() < SyncResult::Success) {
            _overallStatus.setStatus(status);
        }
        break;
    case SyncResult::Success:
        Q_FALLTHROUGH();
    case SyncResult::NotYetStarted:
        Q_FALLTHROUGH();
    case SyncResult::SyncPrepare:
        Q_FALLTHROUGH();
    case SyncResult::SyncRunning:
        if (_overallStatus.status() < SyncResult::Problem) {
            _overallStatus.setStatus(status);
        }
        break;
    case SyncResult::Undefined:
        if (_overallStatus.status() < SyncResult::Problem) {
            _overallStatus.setStatus(SyncResult::Problem);
        }
        break;
    case SyncResult::Problem:
        Q_FALLTHROUGH();
    case SyncResult::Error:
        Q_FALLTHROUGH();
    case SyncResult::SetupError:
        if (_overallStatus.status() < status) {
            _overallStatus.setStatus(status);
        }
        break;
    }
}

const SyncResult &TrayOverallStatusResult::overallStatus() const
{
    return _overallStatus;
}

FolderMan *FolderMan::_instance = nullptr;

FolderMan::FolderMan(QObject *parent)
    : QObject(parent)
    , _currentSyncFolder(nullptr)
    , _syncEnabled(true)
    , _lockWatcher(new LockWatcher)
#ifdef Q_OS_WIN
    , _navigationPaneHelper(this)
#endif
    , _appRestartRequired(false)
{
    OC_ASSERT(!_instance);
    _instance = this;

    _socketApi.reset(new SocketApi);

    // Set the remote poll interval fixed to 10 seconds.
    // That does not mean that it polls every 10 seconds, but it checks every 10 seconds
    // if one of the folders is due to sync. This means that if the server advertises a
    // pollinterval that is not a multiple of 10 seconds, then that pollinterval will be
    // rounded up to the next 10 seconds in practice. 10-second granularity is acceptable.
    _etagPollTimer.setInterval(10s);
    QObject::connect(&_etagPollTimer, &QTimer::timeout, this, &FolderMan::slotEtagPollTimerTimeout);
    _etagPollTimer.start();

    _startScheduledSyncTimer.setSingleShot(true);
    connect(&_startScheduledSyncTimer, &QTimer::timeout,
        this, &FolderMan::slotStartScheduledFolderSync);

    _timeScheduler.setInterval(5000);
    _timeScheduler.setSingleShot(false);
    connect(&_timeScheduler, &QTimer::timeout,
        this, &FolderMan::slotScheduleFolderByTime);
    _timeScheduler.start();

    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &FolderMan::slotRemoveFoldersForAccount);

    connect(_lockWatcher.data(), &LockWatcher::fileUnlocked,
        this, &FolderMan::slotWatchedFileUnlocked);
}

FolderMan *FolderMan::instance()
{
    return _instance;
}

FolderMan::~FolderMan()
{
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
        disconnect(f, &Folder::syncStarted,
            this, &FolderMan::slotFolderSyncStarted);
        disconnect(f, &Folder::syncFinished,
            this, &FolderMan::slotFolderSyncFinished);
        disconnect(f, &Folder::syncStateChange,
            this, &FolderMan::slotForwardFolderSyncStateChange);
        disconnect(f, &Folder::syncPausedChanged,
            this, &FolderMan::slotFolderSyncPaused);
        disconnect(&f->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
            _socketApi.data(), &SocketApi::broadcastStatusPushMessage);
        disconnect(f, &Folder::watchedFileChangedExternally,
            &f->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::slotPathTouched);

        f->syncEngine().disconnect(f);
    }
}

void FolderMan::unloadAndDeleteAllFolders()
{
    // clear the list of existing folders.
    const auto folders = std::move(_folders);
    for (auto *folder : folders) {
        _socketApi->slotUnregisterPath(folder);
        folder->deleteLater();
    }
    _lastSyncFolder = nullptr;
    _currentSyncFolder = nullptr;
    _scheduledFolders.clear();
    emit folderListChanged();
    emit scheduleQueueChanged();
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

int FolderMan::setupFolders()
{
    unloadAndDeleteAllFolders();

    QStringList skipSettingsKeys, deleteSettingsKeys;
    backwardMigrationSettingsKeys(&deleteSettingsKeys, &skipSettingsKeys);
    // deleteKeys should already have been deleted on application startup.
    // We ignore them here just in case.
    skipSettingsKeys += deleteSettingsKeys;

    auto settings = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));
    const auto &accountsWithSettings = settings->childGroups();
    if (accountsWithSettings.isEmpty()) {
        int r = setupFoldersMigration();
        if (r > 0) {
            AccountManager::instance()->save(false); // don't save credentials, they had not been loaded from keychain
        }
        return r;
    }

    qCInfo(lcFolderMan) << "Setup folders from settings file";

    for (const auto &account : AccountManager::instance()->accounts()) {
        const auto id = account->account()->id();
        if (!accountsWithSettings.contains(id)) {
            continue;
        }
        settings->beginGroup(id);

        // The "backwardsCompatible" flag here is related to migrating old
        // database locations
        auto process = [&](const QString &groupName, bool backwardsCompatible, bool foldersWithPlaceholders) {
            settings->beginGroup(groupName);
            if (skipSettingsKeys.contains(settings->group())) {
                // Should not happen: bad container keys should have been deleted
                qCWarning(lcFolderMan) << "Folder structure" << groupName << "is too new, ignoring";
            } else {
                setupFoldersHelper(*settings, account, skipSettingsKeys, backwardsCompatible, foldersWithPlaceholders);
            }
            settings->endGroup();
        };

        process(QStringLiteral("Folders"), true, false);

        // See Folder::saveToSettings for details about why these exists.
        process(QStringLiteral("Multifolders"), false, false);
        process(QStringLiteral("FoldersWithPlaceholders"), false, true);

        settings->endGroup(); // <account>
    }

    emit folderListChanged();

    return _folders.size();
}

void FolderMan::setupFoldersHelper(QSettings &settings, AccountStatePtr account, const QStringList &ignoreKeys, bool backwardsCompatible, bool foldersWithPlaceholders)
{
    const auto &childGroups = settings.childGroups();
    for (const auto &folderAlias : childGroups) {
        // Skip folders with too-new version
        settings.beginGroup(folderAlias);
        if (ignoreKeys.contains(settings.group())) {
            qCInfo(lcFolderMan) << "Folder" << folderAlias << "is too new, ignoring";
            _additionalBlockedFolderAliases.insert(folderAlias);
            settings.endGroup();
            continue;
        }
        settings.endGroup();

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
        if (folderDefinition.journalPath.startsWith("._sync_")
            && defaultJournalPath.startsWith(".sync_")
            && !QFile::exists(folderDefinition.absoluteJournalPath())) {
            folderDefinition.journalPath = defaultJournalPath;
        }

        // Migration: If an old .csync_journal.db is found, move it to the new name.
        if (backwardsCompatible) {
            SyncJournalDb::maybeMigrateDb(folderDefinition.localPath(), folderDefinition.absoluteJournalPath());
        }

        auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
        if (!vfs) {
            // TODO: Must do better error handling
            qFatal("Could not load plugin");
        }

        if (Folder *f = addFolderInternal(std::move(folderDefinition), account, std::move(vfs))) {
            // Migrate the old "usePlaceholders" setting to the root folder pin state
            if (settings.value(versionC(), 1).toInt() == 1
                && settings.value(QLatin1String("usePlaceholders"), false).toBool()) {
                qCInfo(lcFolderMan) << "Migrate: From usePlaceholders to PinState::OnlineOnly";
                f->setRootPinState(PinState::OnlineOnly);
            }

            // Migration: Mark folders that shall be saved in a backwards-compatible way
            if (backwardsCompatible)
                f->setSaveBackwardsCompatible(true);
            if (foldersWithPlaceholders)
                f->setSaveInFoldersWithPlaceholders();

            // save possible changes from the migration
            f->saveToSettings();

            scheduleFolder(f);
            emit folderSyncStateChange(f);
        }
        settings.endGroup();
    }
}

int FolderMan::setupFoldersMigration()
{
    _folderConfigPath = ConfigFile::configPath() + QLatin1String("folders");

    qCInfo(lcFolderMan) << "Setup folders from " << _folderConfigPath << "(migration)";

    QDir dir(_folderConfigPath);
    //We need to include hidden files just in case the alias starts with '.'
    dir.setFilter(QDir::Files | QDir::Hidden);
    const auto &list = dir.entryList();
    OC_ENFORCE_X(list.isEmpty(), "Migration from < 2.0 is no longer supported");
    // return the number of valid folders.
    return _folders.size();
}

void FolderMan::backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    auto settings = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));

    auto processSubgroup = [&](const QString &name) {
        settings->beginGroup(name);
        const int foldersVersion = settings->value(versionC(), 1).toInt();
        if (foldersVersion <= maxFoldersVersion) {
            const auto &childGroups = settings->childGroups();
            for (const auto &folderAlias : childGroups) {
                settings->beginGroup(folderAlias);
                const int folderVersion = settings->value(versionC(), 1).toInt();
                if (folderVersion > FolderDefinition::maxSettingsVersion()) {
                    ignoreKeys->append(settings->group());
                }
                settings->endGroup();
            }
        } else {
            deleteKeys->append(settings->group());
        }
        settings->endGroup();
    };

    const auto &childGroups = settings->childGroups();
    for (const auto &accountId : childGroups) {
        settings->beginGroup(accountId);
        processSubgroup("Folders");
        processSubgroup("Multifolders");
        processSubgroup("FoldersWithPlaceholders");
        settings->endGroup();
    }
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
    return this->_socketApi.data();
}

void FolderMan::slotFolderSyncPaused(Folder *f, bool paused)
{
    if (!f) {
        qCCritical(lcFolderMan) << "slotFolderSyncPaused called with empty folder";
        return;
    }

    if (!paused) {
        _disabledFolders.remove(f);
        scheduleFolder(f);
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
            scheduleFolder(f);
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

/*
  * if a folder wants to be synced, it calls this slot and is added
  * to the queue. The slot to actually start a sync is called afterwards.
  */
void FolderMan::scheduleFolder(Folder *f)
{
    qCInfo(lcFolderMan) << "Schedule folder " << f->path() << " to sync!";

    if (!_scheduledFolders.contains(f)) {
        if (!f->canSync()) {
            qCInfo(lcFolderMan) << "Folder is not ready to sync, not scheduled!";
            _socketApi->slotUpdateFolderView(f);
            return;
        }
        f->prepareToSync();
        emit folderSyncStateChange(f);
        _scheduledFolders.enqueue(f);
        emit scheduleQueueChanged();
    } else {
        qCInfo(lcFolderMan) << "Sync for folder " << f->path() << " already scheduled, do not enqueue!";
    }

    startScheduledSyncSoon();
}

void FolderMan::scheduleFolderNext(Folder *f)
{
    qCInfo(lcFolderMan) << "Schedule folder " << f->path() << " to sync! Front-of-queue.";

    if (!f->canSync()) {
        qCInfo(lcFolderMan) << "Folder is not ready to sync, not scheduled!";
        return;
    }

    _scheduledFolders.removeAll(f);

    f->prepareToSync();
    emit folderSyncStateChange(f);
    _scheduledFolders.prepend(f);
    emit scheduleQueueChanged();

    startScheduledSyncSoon();
}

void FolderMan::slotScheduleETagJob(RequestEtagJob *job)
{
    QObject::connect(job, &QObject::destroyed, this, &FolderMan::slotEtagJobDestroyed);
    QMetaObject::invokeMethod(this, &FolderMan::slotRunOneEtagJob, Qt::QueuedConnection);
    // maybe: add to queue
}

void FolderMan::slotEtagJobDestroyed(QObject * /*o*/)
{
    // _currentEtagJob is automatically cleared
    // maybe: remove from queue
    QMetaObject::invokeMethod(this, &FolderMan::slotRunOneEtagJob, Qt::QueuedConnection);
}

void FolderMan::slotRunOneEtagJob()
{
    if (_currentEtagJob.isNull()) {
        Folder *folder = nullptr;
        for (auto *f : qAsConst(_folders)) {
            if (f->etagJob()) {
                // Caveat: always grabs the first folder with a job, but we think this is Ok for now and avoids us having a seperate queue.
                _currentEtagJob = f->etagJob();
                folder = f;
                break;
            }
        }
        if (_currentEtagJob.isNull()) {
            //qCDebug(lcFolderMan) << "No more remote ETag check jobs to schedule.";

            /* now it might be a good time to check for restarting... */
            if (!isAnySyncRunning() && _appRestartRequired) {
                restartApplication();
            }
        } else {
            qCDebug(lcFolderMan) << "Scheduling" << folder->remoteUrl().toString() << "to check remote ETag";
            _currentEtagJob->start(); // on destroy/end it will continue the queue via slotEtagJobDestroyed
        }
    }
}

void FolderMan::slotAccountStateChanged()
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
                scheduleFolder(f);
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

        QMutableListIterator<Folder *> it(_scheduledFolders);
        while (it.hasNext()) {
            Folder *f = it.next();
            if (f->accountState() == accountState) {
                it.remove();
            }
        }
        emit scheduleQueueChanged();
    }
}

// only enable or disable foldermans will schedule and do syncs.
// this is not the same as Pause and Resume of folders.
void FolderMan::setSyncEnabled(bool enabled)
{
    if (!_syncEnabled && enabled && !_scheduledFolders.isEmpty()) {
        // We have things in our queue that were waiting for the connection to come back on.
        startScheduledSyncSoon();
    }
    qCInfo(lcFolderMan) << Q_FUNC_INFO << enabled;
    _syncEnabled = enabled;
    // force a redraw in case the network connect status changed
    Q_EMIT folderSyncStateChange(nullptr);
}

void FolderMan::startScheduledSyncSoon()
{
    if (_startScheduledSyncTimer.isActive()) {
        return;
    }
    if (_scheduledFolders.empty()) {
        return;
    }
    if (isAnySyncRunning()) {
        return;
    }

    seconds delay { 1s }; // Startup, if _lastSyncFolder is still empty.
    seconds sinceLastSync {};

    // Require a pause based on the duration of the last sync run.
    if (Folder *lastFolder = _lastSyncFolder) {
        sinceLastSync = duration_cast<seconds>(lastFolder->msecSinceLastSync());

        //  1s   -> 1.5s pause
        // 10s   -> 5s pause
        //  1min -> 12s pause
        //  1h   -> 90s pause
        delay = seconds(static_cast<int64_t>(qSqrt(duration_cast<seconds>(lastFolder->msecLastSyncDuration()).count()) / 20));
    } else {
        qDebug() << "Setting initial sync start delay of" << delay.count();
    }

    // Delays beyond one minute seem too big, particularly since there
    // could be things later in the queue that shouldn't be punished by a
    // long delay!
    delay = qBound(1s, delay - sinceLastSync, 60s);
    qCInfo(lcFolderMan) << "Starting the next scheduled sync in" << delay.count() << "seconds";
    _startScheduledSyncTimer.start(delay);
}

/*
  * slot to start folder syncs.
  * It is either called from the slot where folders enqueue themselves for
  * syncing or after a folder sync was finished.
  */
void FolderMan::slotStartScheduledFolderSync()
{
    if (isAnySyncRunning()) {
        for (auto *f : qAsConst(_folders)) {
            if (f->isSyncRunning())
                qCInfo(lcFolderMan) << "Currently folder " << f->remoteUrl().toString() << " is running, wait for finish!";
        }
        return;
    }

    if (!_syncEnabled) {
        qCInfo(lcFolderMan) << "FolderMan: Syncing is disabled, no scheduling.";
        return;
    }

    qCDebug(lcFolderMan) << "folderQueue size: " << _scheduledFolders.count();
    if (_scheduledFolders.isEmpty()) {
        return;
    }

    // Find the first folder in the queue that can be synced.
    Folder *folder = nullptr;
    while (!_scheduledFolders.isEmpty()) {
        Folder *g = _scheduledFolders.dequeue();
        if (g->canSync()) {
            folder = g;
            break;
        }
    }

    emit scheduleQueueChanged();

    // Start syncing this folder!
    if (folder) {
        // Safe to call several times, and necessary to try again if
        // the folder path didn't exist previously.
        folder->registerFolderWatcher();
        registerFolderWithSocketApi(folder);

        _currentSyncFolder = folder;
        qCInfo(lcFolderMan) << "Start scheduled sync of" << folder->path();
        folder->startSync();
    }
}

void FolderMan::slotEtagPollTimerTimeout()
{
    for (auto *f : qAsConst(_folders)) {
        if (!f) {
            continue;
        }
        if (_scheduledFolders.contains(f)) {
            continue;
        }
        if (_disabledFolders.contains(f)) {
            continue;
        }
        if (f->dueToSync()) {
            QMetaObject::invokeMethod(f, &Folder::slotRunEtagJob, Qt::QueuedConnection);
        }
    }
}

void FolderMan::slotRemoveFoldersForAccount(AccountStatePtr accountState)
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

void FolderMan::slotForwardFolderSyncStateChange()
{
    if (Folder *f = qobject_cast<Folder *>(sender())) {
        emit folderSyncStateChange(f);
    }
}

void FolderMan::slotServerVersionChanged(Account *account)
{
    // Pause folders if the server version is unsupported
    if (account->serverVersionUnsupported()) {
        qCWarning(lcFolderMan) << "The server version is unsupported:" << account->capabilities().status().versionString()
                               << "pausing all folders on the account";

        for (auto &f : qAsConst(_folders)) {
            if (f->accountState()->account().data() == account) {
                f->setSyncPaused(true);
            }
        }
    }
}

void FolderMan::slotWatchedFileUnlocked(const QString &path)
{
    if (Folder *f = folderForPath(path)) {
        // Treat this equivalently to the file being reported by the file watcher
        f->slotWatchedPathChanged(path, Folder::ChangeReason::UnLock);
    }
}

void FolderMan::slotScheduleFolderByTime()
{
    for (const auto &f : qAsConst(_folders)) {
        // Never schedule if syncing is disabled or when we're currently
        // querying the server for etags
        if (!f->canSync() || f->etagJob()) {
            continue;
        }

        auto msecsSinceSync = f->msecSinceLastSync();

        // Possibly it's just time for a new sync run
        const auto pta = f->accountState()->account()->capabilities().remotePollInterval();
        bool forceSyncIntervalExpired = msecsSinceSync > ConfigFile().forceSyncInterval(pta);
        if (forceSyncIntervalExpired) {
            qCInfo(lcFolderMan) << "Scheduling folder" << f->path()
                                << "because it has been" << msecsSinceSync.count() << "ms "
                                << "since the last sync";

            scheduleFolder(f);
            continue;
        }

        // Retry a couple of times after failure; or regularly if requested
        bool syncAgain =
            (f->consecutiveFailingSyncs() > 0 && f->consecutiveFailingSyncs() < 3)
            || f->syncEngine().isAnotherSyncNeeded() == DelayedFollowUp;
        auto syncAgainDelay = std::chrono::seconds(10); // 10s for the first retry-after-fail
        if (f->consecutiveFailingSyncs() > 1)
            syncAgainDelay = std::chrono::seconds(60); // 60s for each further attempt
        if (syncAgain && msecsSinceSync > syncAgainDelay) {
            qCInfo(lcFolderMan) << "Scheduling folder" << f->path()
                                << ", the last" << f->consecutiveFailingSyncs() << "syncs failed"
                                << ", anotherSyncNeeded" << f->syncEngine().isAnotherSyncNeeded()
                                << ", last status:" << f->syncResult().statusString()
                                << ", time since last sync:" << msecsSinceSync.count();

            scheduleFolder(f);
            continue;
        }

        // Do we want to retry failing syncs or another-sync-needed runs more often?
    }
}

bool FolderMan::isAnySyncRunning() const
{
    if (_currentSyncFolder)
        return true;

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

    qCInfo(lcFolderMan) << "<========== Sync finished for folder ["
                        << f->shortGuiLocalPath()
                        << "] of account ["
                        << f->accountState()->account()->displayName()
                        << "] with remote ["
                        << f->remoteUrl().toDisplayString()
                        << "]";
    if (f == _currentSyncFolder) {
        _lastSyncFolder = _currentSyncFolder;
        _currentSyncFolder = nullptr;
    }
    if (!isAnySyncRunning())
        startScheduledSyncSoon();
}

Folder *FolderMan::addFolder(AccountStatePtr accountState, const FolderDefinition &folderDefinition)
{
    // Choose a db filename
    auto definition = folderDefinition;
    definition.journalPath = SyncJournalDb::makeDbName(folderDefinition.localPath());

    if (!ensureJournalGone(definition.absoluteJournalPath())) {
        return nullptr;
    }

    auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
    if (!vfs) {
        qCWarning(lcFolderMan) << "Could not load plugin for mode" << folderDefinition.virtualFilesMode;
        return nullptr;
    }

    auto folder = addFolderInternal(definition, accountState, std::move(vfs));

    // Migration: The first account that's configured for a local folder shall
    // be saved in a backwards-compatible way.
    bool oneAccountOnly = true;
    for (auto *other : _folders) {
        if (other != folder && other->cleanPath() == folder->cleanPath()) {
            oneAccountOnly = false;
            break;
        }
    }

    if (folder) {
        folder->setSaveBackwardsCompatible(oneAccountOnly);
        folder->saveToSettings();
        emit folderSyncStateChange(folder);
        emit folderListChanged();
    }

#ifdef Q_OS_WIN
    _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif
    return folder;
}

Folder *FolderMan::addFolderInternal(
    FolderDefinition folderDefinition,
    AccountStatePtr accountState,
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
        connect(folder, &Folder::syncStarted, this, &FolderMan::slotFolderSyncStarted);
        connect(folder, &Folder::syncFinished, this, &FolderMan::slotFolderSyncFinished);
        connect(folder, &Folder::syncStateChange, this, &FolderMan::slotForwardFolderSyncStateChange);
        connect(folder, &Folder::syncPausedChanged, this, &FolderMan::slotFolderSyncPaused);
        connect(folder, &Folder::canSyncChanged, this, &FolderMan::slotFolderCanSyncChanged);
        connect(&folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
            _socketApi.data(), &SocketApi::broadcastStatusPushMessage);
        connect(folder, &Folder::watchedFileChangedExternally,
            &folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::slotPathTouched);

        folder->registerFolderWatcher();
        registerFolderWithSocketApi(folder);
    }
    return folder;
}

Folder *FolderMan::folderForPath(const QString &path, QString *relativePath)
{
    QString absolutePath = QDir::cleanPath(path) + QLatin1Char('/');

    for (auto *folder : _folders) {
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
    if (!serverPath.startsWith('/'))
        serverPath.prepend('/');

    for (auto *folder : _folders) {
        if (acc != nullptr && folder->accountState()->account() != acc) {
            continue;
        }
        if (!serverPath.startsWith(folder->remotePath()))
            continue;

        QString path = folder->cleanPath() + '/';
        path += serverPath.midRef(folder->remotePathTrailingSlash().length());
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

    if (_scheduledFolders.removeAll(f) > 0) {
        emit scheduleQueueChanged();
    }

    f->setSyncPaused(true);
    f->wipeForRemoval();

    // remove the folder configuration
    f->removeFromSettings();

    unloadFolder(f);
    if (currentlyRunning) {
        // We want to schedule the next folder once this is done
        connect(f, &Folder::syncFinished,
            this, &FolderMan::slotFolderSyncFinished);
        // Let the folder delete itself when done.
        connect(f, &Folder::syncFinished, f, &QObject::deleteLater);
    } else {
        f->deleteLater();
    }

#ifdef Q_OS_WIN
    _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif
    Q_EMIT folderRemoved(f);
    emit folderListChanged();
}

QString FolderMan::getBackupName(QString fullPathName) const
{
    if (fullPathName.endsWith("/"))
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

bool FolderMan::startFromScratch(const QString &localFolder)
{
    if (localFolder.isEmpty()) {
        return false;
    }

    QFileInfo fi(localFolder);
    QDir parentDir(fi.dir());
    QString folderName = fi.fileName();

    // Adjust for case where localFolder ends with a /
    if (fi.isDir()) {
        folderName = parentDir.dirName();
        parentDir.cdUp();
    }

    if (fi.exists()) {
        // It exists, but is empty -> just reuse it.
        if (fi.isDir() && fi.dir().count() == 0) {
            qCDebug(lcFolderMan) << "startFromScratch: Directory is empty!";
            return true;
        }
        // Disconnect the socket api from the database to avoid that locking of the
        // db file does not allow to move this dir.
        Folder *f = folderForPath(localFolder);
        if (f) {
            if (localFolder.startsWith(f->path())) {
                _socketApi->slotUnregisterPath(f);
            }
            f->journalDb()->close();
            f->slotTerminateSync(); // Normally it should not be running, but viel hilft viel
        }

        // Make a backup of the folder/file.
        QString newName = getBackupName(parentDir.absoluteFilePath(folderName));
        QString renameError;
        if (!FileSystem::rename(fi.absoluteFilePath(), newName, &renameError)) {
            qCWarning(lcFolderMan) << "startFromScratch: Could not rename" << fi.absoluteFilePath()
                                   << "to" << newName << "error:" << renameError;
            return false;
        }
    }

    if (!parentDir.mkdir(fi.absoluteFilePath())) {
        qCWarning(lcFolderMan) << "startFromScratch: Could not mkdir" << fi.absoluteFilePath();
        return false;
    }

    return true;
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
    case SyncResult::Undefined:
        folderMessage = tr("Undefined State.");
        break;
    case SyncResult::NotYetStarted:
        folderMessage = tr("Waiting to start syncing.");
        break;
    case SyncResult::SyncPrepare:
        folderMessage = tr("Preparing for sync.");
        break;
    case SyncResult::SyncRunning:
        folderMessage = tr("Sync is running.");
        break;
    case SyncResult::Success:
    case SyncResult::Problem:
        if (result.hasUnresolvedConflicts()) {
            folderMessage = tr("Sync was successful, unresolved conflicts.");
        } else {
            folderMessage = tr("Last Sync was successful.");
        }
        break;
    case SyncResult::Error:
        break;
    case SyncResult::SetupError:
        folderMessage = tr("Setup Error.");
        break;
    case SyncResult::SyncAbortRequested:
        folderMessage = tr("User Abort.");
        break;
    case SyncResult::Paused:
        folderMessage = tr("Sync is paused.");
        break;
        // no default case on purpose, check compiler warnings
    }
    if (paused) {
        // sync is disabled.
        folderMessage = tr("%1 (Sync is paused)").arg(folderMessage);
    }
    return folderMessage;
}

static QString checkPathValidityRecursive(const QString &path)
{
    if (path.isEmpty()) {
        return FolderMan::tr("No valid folder selected!");
    }

#ifdef Q_OS_WIN
    Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
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

        return canonicalPath(parentPath) + '/' + selFile.fileName();
    }
    return selFile.canonicalFilePath();
}

QString FolderMan::checkPathValidityForNewFolder(const QString &path) const
{
    // check if the local directory isn't used yet in another ownCloud sync
    const auto cs = Utility::fsCaseSensitivity();

    const QString userDir = QDir::cleanPath(canonicalPath(path)) + '/';
    for (auto f : _folders) {
        const QString folderDir = QDir::cleanPath(canonicalPath(f->path())) + '/';

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

QString FolderMan::findGoodPathForNewSyncFolder(const QString &basePath) const
{
    QString folder = canonicalPath(basePath);

    // If the parent folder is a sync folder or contained in one, we can't
    // possibly find a valid sync folder inside it.
    // Example: Someone syncs their home directory. Then ~/foobar is not
    // going to be an acceptable sync folder path for any value of foobar.
    const QString parentFolder = QFileInfo(folder).canonicalPath();
    if (FolderMan::instance()->folderForPath(parentFolder)) {
        // Any path with that parent is going to be unacceptable,
        // so just keep it as-is.
        return basePath;
    }
    // Count attempts and give up eventually
    for (int attempt = 2; attempt < 100; ++attempt) {
        if (!QFileInfo::exists(folder)
            && FolderMan::instance()->checkPathValidityForNewFolder(folder).isEmpty()) {
            return folder;
        }
        folder = basePath + QString::number(attempt);
    }
    return basePath;
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

QQueue<Folder *> FolderMan::scheduleQueue() const
{
    return _scheduledFolders;
}

Folder *FolderMan::currentSyncFolder() const
{
    return _currentSyncFolder;
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

Folder *FolderMan::addFolderFromWizard(AccountStatePtr accountStatePtr, const QString &localFolder, const QString &remotePath, const QUrl &webDavUrl, const QString &displayName, bool useVfs)
{
    FolderMan::prepareFolder(localFolder);

    qCInfo(lcFolderMan) << "Adding folder definition for" << localFolder << remotePath;
    auto folderDefinition = FolderDefinition::createNewFolderDefinition(webDavUrl, displayName);
    folderDefinition.setLocalPath(localFolder);
    folderDefinition.setTargetPath(remotePath);
    folderDefinition.ignoreHiddenFiles = ignoreHiddenFiles();

    if (useVfs) {
        folderDefinition.virtualFilesMode = bestAvailableVfsMode();
    }

    auto newFolder = addFolder(accountStatePtr, folderDefinition);

    if (newFolder) {
        // With spaces we only handle the main folder
        if (!newFolder->groupInSidebar()) {
            Utility::setupFavLink(localFolder);
#ifdef Q_OS_WIN
            // TODO: move to setupFavLink
            // TODO: don't call setupFavLinkt if showInExplorerNavigationPane is false?
            if (_navigationPaneHelper.showInExplorerNavigationPane())
                folderDefinition.navigationPaneClsid = QUuid::createUuid();
#endif
        }
        if (folderDefinition.virtualFilesMode != Vfs::Off && useVfs)
            newFolder->setRootPinState(PinState::OnlineOnly);

        if (!OwncloudWizard::isConfirmBigFolderChecked()) {
            // The user already accepted the selective sync dialog. everything is in the white list
            newFolder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
                QStringList() << QLatin1String("/"));
        }
        qCDebug(lcFolderMan) << "Local sync folder" << localFolder << "successfully created!";
    } else {
        qCWarning(lcFolderMan) << "Failed to create local sync folder!";
    }
    return newFolder;
}

QString FolderMan::suggestSyncFolder(const QUrl &server, const QString &displayName)
{
    return FolderMan::instance()->findGoodPathForNewSyncFolder(
        QDir::homePath() + QDir::separator() + tr("%1 - %2@%3").arg(OCC::Theme::instance()->defaultClientFolder(), displayName, server.host()));
}

bool FolderMan::prepareFolder(const QString &folder)
{
    if (!QFileInfo::exists(folder)) {
        if (!OC_ENSURE(QDir().mkpath(folder))) {
            return false;
        }
        FileSystem::setFolderMinimumPermissions(folder);

#ifdef Q_OS_WIN
        // First create a Desktop.ini so that the folder and favorite link show our application's icon.
        // TODO: as we only write the file once the path to owncloud.exe can be outdated
        QFile desktopIni(folder + QStringLiteral("/Desktop.ini"));
        if (desktopIni.exists()) {
            qCWarning(lcFolderMan) << desktopIni.fileName() << "already exists, not overwriting it to set the folder icon.";
        } else {
            qCInfo(lcFolderMan) << "Creating" << desktopIni.fileName() << "to set a folder icon in Explorer.";
            if (OC_ENSURE(desktopIni.open(QFile::WriteOnly))) {
                desktopIni.write("[.ShellClassInfo]\r\nIconResource=");
                desktopIni.write(QDir::toNativeSeparators(qApp->applicationFilePath()).toUtf8());
                desktopIni.write(",0\r\n");
                desktopIni.close();
            }

            const QString longFolderPath = FileSystem::longWinPath(folder);
            const QString longDesktopIniPath = FileSystem::longWinPath(desktopIni.fileName());
            // Set the folder as system and Desktop.ini as hidden+system for explorer to pick it.
            // https://msdn.microsoft.com/en-us/library/windows/desktop/cc144102
            const DWORD folderAttrs = GetFileAttributesW(reinterpret_cast<const wchar_t *>(longFolderPath.utf16()));
            SetFileAttributesW(reinterpret_cast<const wchar_t *>(longFolderPath.utf16()), folderAttrs | FILE_ATTRIBUTE_SYSTEM);
            SetFileAttributesW(reinterpret_cast<const wchar_t *>(longDesktopIniPath.utf16()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        }
#endif
    }
    return true;
}

} // namespace OCC
