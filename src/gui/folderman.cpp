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
#include "configfile.h"
#include "folder.h"
#include "syncresult.h"
#include "theme.h"
#include "socketapi.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "filesystem.h"
#include "lockwatcher.h"
#include <syncengine.h>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include <QMessageBox>
#include <QtCore>
#include <QMutableSetIterator>
#include <QSet>

namespace OCC {

FolderMan* FolderMan::_instance = 0;

FolderMan::FolderMan(QObject *parent) :
    QObject(parent),
    _currentSyncFolder(0),
    _syncEnabled( true ),
    _lockWatcher(new LockWatcher),
    _appRestartRequired(false)
{
    Q_ASSERT(!_instance);
    _instance = this;

    _socketApi.reset(new SocketApi);

    ConfigFile cfg;
    int polltime = cfg.remotePollInterval();
    qDebug() << "setting remote poll timer interval to" << polltime << "msec";
    _etagPollTimer.setInterval( polltime );
    QObject::connect(&_etagPollTimer, SIGNAL(timeout()), this, SLOT(slotEtagPollTimerTimeout()));
    _etagPollTimer.start();

    _startScheduledSyncTimer.setSingleShot(true);
    connect(&_startScheduledSyncTimer, SIGNAL(timeout()),
            SLOT(slotStartScheduledFolderSync()));

    connect(AccountManager::instance(), SIGNAL(accountRemoved(AccountState*)),
            SLOT(slotRemoveFoldersForAccount(AccountState*)));

    connect(_lockWatcher.data(), SIGNAL(fileUnlocked(QString)),
            SLOT(slotScheduleFolderOwningFile(QString)));
}

FolderMan *FolderMan::instance()
{
    return _instance;
}

FolderMan::~FolderMan()
{
    qDeleteAll(_folderMap);
    _instance = 0;
}

OCC::Folder::Map FolderMan::map()
{
    return _folderMap;
}

void FolderMan::unloadFolder( Folder *f )
{
    if( !f ) {
        return;
    }

    _socketApi->slotUnregisterPath(f->alias());

    if( _folderWatchers.contains(f->alias())) {
        _folderWatchers.remove(f->alias());
    }
    _folderMap.remove( f->alias() );

    disconnect(f, SIGNAL(scheduleToSync(Folder*)),
               this, SLOT(slotScheduleSync(Folder*)));
    disconnect(f, SIGNAL(syncStarted()),
               this, SLOT(slotFolderSyncStarted()));
    disconnect(f, SIGNAL(syncFinished(SyncResult)),
               this, SLOT(slotFolderSyncFinished(SyncResult)));
    disconnect(f, SIGNAL(syncStateChange()),
               this, SLOT(slotForwardFolderSyncStateChange()));
    disconnect(f, SIGNAL(syncPausedChanged(Folder*,bool)),
               this, SLOT(slotFolderSyncPaused(Folder*,bool)));
    disconnect(&f->syncEngine().syncFileStatusTracker(), SIGNAL(fileStatusChanged(const QString &, SyncFileStatus)),
               _socketApi.data(), SLOT(slotFileStatusChanged(const QString &, SyncFileStatus)));
    disconnect(f, SIGNAL(watchedFileChangedExternally(QString)),
               &f->syncEngine().syncFileStatusTracker(), SLOT(slotPathTouched(QString)));
}

int FolderMan::unloadAndDeleteAllFolders()
{
    int cnt = 0;

    // clear the list of existing folders.
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder* f = i.value();
        unloadFolder(f);
        delete f;
        cnt++;
    }
    _lastSyncFolder = 0;
    _currentSyncFolder = 0;
    _scheduleQueue.clear();
    emit scheduleQueueChanged();

    Q_ASSERT(_folderMap.count() == 0);
    return cnt;
}

// add a monitor to the local file system. If there is a change in the
// file system, the method slotFolderMonitorFired is triggered through
// the SignalMapper
void FolderMan::registerFolderMonitor( Folder *folder )
{
    if( !folder ) return;

    if( !_folderWatchers.contains(folder->alias() ) ) {
        FolderWatcher *fw = new FolderWatcher(folder->path(), folder);

        // Connect the pathChanged signal, which comes with the changed path,
        // to the signal mapper which maps to the folder alias. The changed path
        // is lost this way, but we do not need it for the current implementation.
        connect(fw, SIGNAL(pathChanged(QString)), folder, SLOT(slotWatchedPathChanged(QString)));

        _folderWatchers.insert(folder->alias(), fw);
    }

    // register the folder with the socket API
    if (folder->canSync())
        _socketApi->slotRegisterPath(folder->alias());
}

void FolderMan::addMonitorPath( const QString& alias, const QString& path )
{
    if( !alias.isEmpty() && _folderWatchers.contains(alias) ) {
        FolderWatcher *fw = _folderWatchers[alias];

        if( fw ) {
            fw->addPath(path);
        }
    }
}

void FolderMan::removeMonitorPath( const QString& alias, const QString& path )
{
    if( !alias.isEmpty() && _folderWatchers.contains(alias) ) {
        FolderWatcher *fw = _folderWatchers[alias];

        if( fw ) {
            fw->removePath(path);
        }
    }
}

int FolderMan::setupFolders()
{
    unloadAndDeleteAllFolders();

    auto settings = Account::settingsWithGroup(QLatin1String("Accounts"));
    const auto accountsWithSettings = settings->childGroups();
    if (accountsWithSettings.isEmpty()) {
        int r = setupFoldersMigration();
        if (r > 0) {
            AccountManager::instance()->save(false); // don't save credentials, they had not been loaded from keychain
        }
        return r;
    }

    qDebug() << "* Setup folders from settings file";

    foreach (const auto& account, AccountManager::instance()->accounts()) {
        const auto id = account->account()->id();
        if (!accountsWithSettings.contains(id)) {
            continue;
        }
        settings->beginGroup(id);
        settings->beginGroup(QLatin1String("Folders"));
        foreach (const auto& folderAlias, settings->childGroups()) {
            FolderDefinition folderDefinition;
            if (FolderDefinition::load(*settings, folderAlias, &folderDefinition)) {
                Folder* f = addFolderInternal(std::move(folderDefinition), account.data());
                if (f) {
                    slotScheduleSync(f);
                    emit folderSyncStateChange(f);
                }
            }
        }
        settings->endGroup(); // Folders
        settings->endGroup(); // <account>
    }

    emit folderListChanged(_folderMap);

    return _folderMap.size();
}

int FolderMan::setupFoldersMigration()
{
    ConfigFile cfg;
    QDir storageDir(cfg.configPath());
    storageDir.mkpath(QLatin1String("folders"));
    _folderConfigPath = cfg.configPath() + QLatin1String("folders");

    qDebug() << "* Setup folders from " << _folderConfigPath << "(migration)";

    QDir dir( _folderConfigPath );
    //We need to include hidden files just in case the alias starts with '.'
    dir.setFilter(QDir::Files | QDir::Hidden);
    QStringList list = dir.entryList();

    // Normally there should be only one account when migrating.
    AccountState* accountState = AccountManager::instance()->accounts().value(0).data();
    foreach ( const QString& alias, list ) {
        Folder *f = setupFolderFromOldConfigFile( alias, accountState );
        if( f ) {
            slotScheduleSync(f);
            emit folderSyncStateChange(f);
        }
    }

    emit folderListChanged(_folderMap);

    // return the number of valid folders.
    return _folderMap.size();
}

bool FolderMan::ensureJournalGone( const QString& journalDbFile )
{
    // remove the old journal file
    while (QFile::exists(journalDbFile) && !QFile::remove(journalDbFile)) {
        qDebug() << "Could not remove old db file at" << journalDbFile;
        int ret = QMessageBox::warning(0, tr("Could not reset folder state"),
                                       tr("An old sync journal '%1' was found, "
                                          "but could not be removed. Please make sure "
                                          "that no application is currently using it.")
                                       .arg(QDir::fromNativeSeparators(QDir::cleanPath(journalDbFile))),
                                       QMessageBox::Retry|QMessageBox::Abort);
        if (ret == QMessageBox::Abort) {
            return false;
        }
    }
    return true;
}

#define SLASH_TAG   QLatin1String("__SLASH__")
#define BSLASH_TAG  QLatin1String("__BSLASH__")
#define QMARK_TAG   QLatin1String("__QMARK__")
#define PERCENT_TAG QLatin1String("__PERCENT__")
#define STAR_TAG    QLatin1String("__STAR__")
#define COLON_TAG   QLatin1String("__COLON__")
#define PIPE_TAG    QLatin1String("__PIPE__")
#define QUOTE_TAG   QLatin1String("__QUOTE__")
#define LT_TAG      QLatin1String("__LESS_THAN__")
#define GT_TAG      QLatin1String("__GREATER_THAN__")
#define PAR_O_TAG   QLatin1String("__PAR_OPEN__")
#define PAR_C_TAG   QLatin1String("__PAR_CLOSE__")

QString FolderMan::escapeAlias( const QString& alias )
{
    QString a(alias);

    a.replace( QLatin1Char('/'), SLASH_TAG );
    a.replace( QLatin1Char('\\'), BSLASH_TAG );
    a.replace( QLatin1Char('?'), QMARK_TAG  );
    a.replace( QLatin1Char('%'), PERCENT_TAG );
    a.replace( QLatin1Char('*'), STAR_TAG );
    a.replace( QLatin1Char(':'), COLON_TAG );
    a.replace( QLatin1Char('|'), PIPE_TAG );
    a.replace( QLatin1Char('"'), QUOTE_TAG );
    a.replace( QLatin1Char('<'), LT_TAG );
    a.replace( QLatin1Char('>'), GT_TAG );
    a.replace( QLatin1Char('['), PAR_O_TAG );
    a.replace( QLatin1Char(']'), PAR_C_TAG );
    return a;
}

SocketApi *FolderMan::socketApi()
{
    return this->_socketApi.data();
}

QString FolderMan::unescapeAlias( const QString& alias )
{
    QString a(alias);

    a.replace( SLASH_TAG,   QLatin1String("/") );
    a.replace( BSLASH_TAG,  QLatin1String("\\") );
    a.replace( QMARK_TAG,   QLatin1String("?")  );
    a.replace( PERCENT_TAG, QLatin1String("%") );
    a.replace( STAR_TAG,    QLatin1String("*") );
    a.replace( COLON_TAG,   QLatin1String(":") );
    a.replace( PIPE_TAG,    QLatin1String("|") );
    a.replace( QUOTE_TAG,   QLatin1String("\"") );
    a.replace( LT_TAG,      QLatin1String("<") );
    a.replace( GT_TAG,      QLatin1String(">") );
    a.replace( PAR_O_TAG,   QLatin1String("[") );
    a.replace( PAR_C_TAG,   QLatin1String("]") );

    return a;
}

// filename is the name of the file only, it does not include
// the configuration directory path
Folder* FolderMan::setupFolderFromOldConfigFile(const QString &file, AccountState *accountState )
{
    Folder *folder = 0;

    qDebug() << "  ` -> setting up:" << file;
    QString escapedAlias(file);
    // check the unescaped variant (for the case when the filename comes out
    // of the directory listing). If the file does not exist, escape the
    // file and try again.
    QFileInfo cfgFile( _folderConfigPath, file);

    if( !cfgFile.exists() ) {
        // try the escaped variant.
        escapedAlias = escapeAlias(file);
        cfgFile.setFile( _folderConfigPath, escapedAlias );
    }
    if( !cfgFile.isReadable() ) {
        qDebug() << "Cannot read folder definition for alias " << cfgFile.filePath();
        return folder;
    }

    QSettings settings( _folderConfigPath + QLatin1Char('/') + escapedAlias, QSettings::IniFormat);
    qDebug() << "    -> file path: " << settings.fileName();

    // Check if the filename is equal to the group setting. If not, use the group
    // name as an alias.
    QStringList groups = settings.childGroups();

    if( ! groups.contains(escapedAlias) && groups.count() > 0 ) {
        escapedAlias = groups.first();
    }

    settings.beginGroup( escapedAlias ); // read the group with the same name as the file which is the folder alias

    QString path = settings.value(QLatin1String("localPath")).toString();
    QString backend = settings.value(QLatin1String("backend")).toString();
    QString targetPath = settings.value( QLatin1String("targetPath")).toString();
    bool paused = settings.value( QLatin1String("paused"), false).toBool();
    // QString connection = settings.value( QLatin1String("connection") ).toString();
    QString alias = unescapeAlias( escapedAlias );

    if (backend.isEmpty() || backend != QLatin1String("owncloud")) {
        qWarning() << "obsolete configuration of type" << backend;
        return 0;
    }

    // cut off the leading slash, oCUrl always has a trailing.
    if( targetPath.startsWith(QLatin1Char('/')) ) {
        targetPath.remove(0,1);
    }

    if (!accountState) {
        qWarning() << "can't create folder without an account";
        return 0;
    }

    FolderDefinition folderDefinition;
    folderDefinition.alias = alias;
    folderDefinition.localPath = path;
    folderDefinition.targetPath = targetPath;
    folderDefinition.paused = paused;
    folderDefinition.ignoreHiddenFiles = ignoreHiddenFiles();

    folder = addFolderInternal(folderDefinition, accountState);
    if (folder) {
        QStringList blackList = settings.value( QLatin1String("blackList")).toStringList();
        if (!blackList.empty()) {
            //migrate settings
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);
            settings.remove(QLatin1String("blackList"));
        }

        folder->saveToSettings();
    }
    qDebug() << "Migrated!" << folder;
    settings.sync();
    return folder;
}

void FolderMan::slotFolderSyncPaused( Folder *f, bool paused )
{
    if( !f ) {
        qWarning() << "!! slotFolderSyncPaused called with empty folder";
        return;
    }

    if (!paused) {
        _disabledFolders.remove(f);
        slotScheduleSync(f);
    } else {
        _disabledFolders.insert(f);
    }
}

void FolderMan::slotFolderCanSyncChanged()
{
    Folder *f = qobject_cast<Folder*>(sender());
    Q_ASSERT(f);
    if (f->canSync()) {
        _socketApi->slotRegisterPath(f->alias());
    } else {
        _socketApi->slotUnregisterPath(f->alias());
    }
}

// this really terminates the current sync process
// ie. no questions, no prisoners
// csync still remains in a stable state, regardless of that.
void FolderMan::terminateSyncProcess()
{
    Folder *f = _currentSyncFolder;
    if( f ) {
        // This will, indirectly and eventually, call slotFolderSyncFinished
        // and thereby clear _currentSyncFolder.
        f->slotTerminateSync();
    }
}

Folder *FolderMan::folder( const QString& alias )
{
    if( !alias.isEmpty() ) {
        if( _folderMap.contains( alias )) {
            return _folderMap[alias];
        }
    }
    return 0;
}

void FolderMan::slotScheduleAllFolders()
{
    foreach( Folder *f, _folderMap.values() ) {
        if (f && f->canSync()) {
            slotScheduleSync( f );
        }
    }
}

void FolderMan::slotScheduleAppRestart()
{
    _appRestartRequired = true;
    qDebug() << "## Application restart requested!";
}

void FolderMan::slotSyncOnceFileUnlocks(const QString& path)
{
    _lockWatcher->addFile(path);
}

/*
  * if a folder wants to be synced, it calls this slot and is added
  * to the queue. The slot to actually start a sync is called afterwards.
  */
void FolderMan::slotScheduleSync( Folder *f )
{
    if( !f ) {
        qWarning() << "slotScheduleSync called with null folder";
        return;
    }
    auto alias = f->alias();

    qDebug() << "Schedule folder " << alias << " to sync!";

    if( ! _scheduleQueue.contains(f) ) {
        if( !f->canSync() ) {
            qDebug() << "Folder is not ready to sync, not scheduled!";
            _socketApi->slotUpdateFolderView(f);
            return;
        }
        f->prepareToSync();
        emit folderSyncStateChange(f);
        _scheduleQueue.enqueue(f);
        emit scheduleQueueChanged();
    } else {
        qDebug() << " II> Sync for folder " << alias << " already scheduled, do not enqueue!";
    }

    startScheduledSyncSoon();
}

void FolderMan::slotScheduleETagJob(const QString &/*alias*/, RequestEtagJob *job)
{
    QObject::connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotEtagJobDestroyed(QObject*)));
    QMetaObject::invokeMethod(this, "slotRunOneEtagJob", Qt::QueuedConnection);
    // maybe: add to queue
}

void FolderMan::slotEtagJobDestroyed(QObject* /*o*/)
{
    // _currentEtagJob is automatically cleared
    // maybe: remove from queue
    QMetaObject::invokeMethod(this, "slotRunOneEtagJob", Qt::QueuedConnection);
}

void FolderMan::slotRunOneEtagJob()
{
    if (_currentEtagJob.isNull()) {
        Folder *folder;
        foreach(Folder *f, _folderMap) {
            if (f->etagJob()) {
                // Caveat: always grabs the first folder with a job, but we think this is Ok for now and avoids us having a seperate queue.
                _currentEtagJob = f->etagJob();
                folder = f;
                break;
            }
        }
        if (_currentEtagJob.isNull()) {
            //qDebug() << "No more remote ETag check jobs to schedule.";

            /* now it might be a good time to check for restarting... */
            if( _currentSyncFolder == NULL && _appRestartRequired ) {
                restartApplication();
            }
        } else {
            qDebug() << "Scheduling" << folder->remoteUrl().toString() << "to check remote ETag";
            _currentEtagJob->start(); // on destroy/end it will continue the queue via slotEtagJobDestroyed
        }
    }
}

void FolderMan::slotAccountStateChanged()
{
    AccountState * accountState = qobject_cast<AccountState*>(sender());
    if (! accountState) {
        return;
    }
    QString accountName = accountState->account()->displayName();

    if (accountState->isConnected()) {
        qDebug() << "Account" << accountName << "connected, scheduling its folders";

        foreach (Folder *f, _folderMap.values()) {
            if (f
                    && f->canSync()
                    && f->accountState() == accountState) {
                slotScheduleSync(f);
            }
        }
    } else {
        qDebug() << "Account" << accountName << "disconnected or paused, "
                    "terminating or descheduling sync folders";

        if (_currentSyncFolder
                && _currentSyncFolder->accountState() == accountState) {
            _currentSyncFolder->slotTerminateSync();
        }

        QMutableListIterator<Folder*> it(_scheduleQueue);
        while (it.hasNext()) {
            Folder* f = it.next();
            if (f->accountState() == accountState) {
                it.remove();
            }
        }
        emit scheduleQueueChanged();
    }
}

// only enable or disable foldermans will schedule and do syncs.
// this is not the same as Pause and Resume of folders.
void FolderMan::setSyncEnabled( bool enabled )
{
    if (!_syncEnabled && enabled && !_scheduleQueue.isEmpty()) {
        // We have things in our queue that were waiting for the connection to come back on.
        startScheduledSyncSoon();
    }
    _syncEnabled = enabled;
    // force a redraw in case the network connect status changed
    emit( folderSyncStateChange(0) );
}

void FolderMan::startScheduledSyncSoon(qint64 msMinimumDelay)
{
    if (_startScheduledSyncTimer.isActive()) {
        return;
    }
    if (_scheduleQueue.empty()) {
        return;
    }
    if (_currentSyncFolder) {
        return;
    }

    qint64 msDelay = msMinimumDelay;
    qint64 msSinceLastSync = 0;

    // Require a pause based on the duration of the last sync run.
    if (Folder* lastFolder = _lastSyncFolder) {
        msSinceLastSync = lastFolder->msecSinceLastSync();

        //  1s   -> 1.5s pause
        // 10s   -> 5s pause
        //  1min -> 12s pause
        //  1h   -> 90s pause
        qint64 pause = qSqrt(lastFolder->msecLastSyncDuration()) / 20.0 * 1000.0;
        msDelay = qMax(msDelay, pause);
    }

    // Punish consecutive follow-up syncs with longer delays.
    if (Folder* nextFolder = _scheduleQueue.head()) {
        int followUps = nextFolder->consecutiveFollowUpSyncs();
        if (followUps >= 2) {
            // This is okay due to the 1min maximum delay limit below.
            msDelay *= qPow(followUps, 2);
        }
    }

    // Delays beyond one minute seem too big, particularly since there
    // could be things later in the queue that shouldn't be punished by a
    // long delay!
    msDelay = qMin(msDelay, 60*1000ll);

    // Time since the last sync run counts against the delay
    msDelay = qMax(1ll, msDelay - msSinceLastSync);

    // A minimum of delay here is essential as the sync will not upload
    // files that were changed too recently.
    msDelay = qMax(SyncEngine::minimumFileAgeForUpload, msDelay);

    qDebug() << "Scheduling a sync in" << (msDelay/1000) << "seconds";
    _startScheduledSyncTimer.start(msDelay);
}

/*
  * slot to start folder syncs.
  * It is either called from the slot where folders enqueue themselves for
  * syncing or after a folder sync was finished.
  */
void FolderMan::slotStartScheduledFolderSync()
{
    if( _currentSyncFolder ) {
        qDebug() << "Currently folder " << _currentSyncFolder->remoteUrl().toString() << " is running, wait for finish!";
        return;
    }

    if( ! _syncEnabled ) {
        qDebug() << "FolderMan: Syncing is disabled, no scheduling.";
        return;
    }

    qDebug() << "XX slotScheduleFolderSync: folderQueue size: " << _scheduleQueue.count();
    if( _scheduleQueue.isEmpty() ) {
        return;
    }

    // Find the first folder in the queue that can be synced.
    Folder* f = 0;
    while( !_scheduleQueue.isEmpty() ) {
        f = _scheduleQueue.dequeue();
        Q_ASSERT(f);

        if( f->canSync() ) {
            break;
        }
    }

    emit scheduleQueueChanged();

    // Start syncing this folder!
    if( f ) {
        _currentSyncFolder = f;
        f->startSync( QStringList() );
    }
}

void FolderMan::slotEtagPollTimerTimeout()
{
    //qDebug() << Q_FUNC_INFO << "Checking if we need to make any folders check the remote ETag";
    ConfigFile cfg;
    int polltime = cfg.remotePollInterval();

    foreach (Folder *f, _folderMap) {
        if (!f) {
            continue;
        }
        if (_currentSyncFolder == f) {
            continue;
        }
        if (_scheduleQueue.contains(f)) {
            continue;
        }
        if (_disabledFolders.contains(f)) {
            continue;
        }
        if (f->etagJob() || f->isBusy() || !f->canSync()) {
            continue;
        }
        if (f->msecSinceLastSync() < polltime) {
            continue;
        }
        QMetaObject::invokeMethod(f, "slotRunEtagJob", Qt::QueuedConnection);
    }
}

void FolderMan::slotRemoveFoldersForAccount(AccountState* accountState)
{
    QVarLengthArray<Folder *, 16> foldersToRemove;
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder* folder = i.value();
        if (folder->accountState() == accountState) {
            foldersToRemove.append(folder);
        }
    }

    foreach (const auto &f, foldersToRemove) {
        slotRemoveFolder(f);
    }
}

void FolderMan::slotForwardFolderSyncStateChange()
{
    if (Folder* f = qobject_cast<Folder*>(sender())) {
        emit folderSyncStateChange(f);
    }
}

void FolderMan::slotServerVersionChanged(Account *account)
{
    // Pause folders if the server version is unsupported
    if (account->serverVersionUnsupported()) {
        qDebug() << "The server version is unsupported:" << account->serverVersion()
                 << "pausing all folders on the account";

        foreach (auto& f, _folderMap) {
            if (f->accountState()->account().data() == account) {
                f->setSyncPaused(true);
            }
        }
    }
}

void FolderMan::slotScheduleFolderOwningFile(const QString& path)
{
    if (Folder* f = folderForPath(path)) {
        slotScheduleSync(f);
    }
}

void FolderMan::slotFolderSyncStarted( )
{
    qDebug() << ">===================================== sync started for " << _currentSyncFolder->remoteUrl().toString();
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  * This delay is particularly useful to avoid late file change notifications
  * (that we caused ourselves by syncing) from triggering another spurious sync.
  */
void FolderMan::slotFolderSyncFinished( const SyncResult& )
{
    qDebug() << "<===================================== sync finished for " << _currentSyncFolder->remoteUrl().toString();

    _lastSyncFolder = _currentSyncFolder;
    _currentSyncFolder = 0;

    startScheduledSyncSoon();
}

Folder* FolderMan::addFolder(AccountState* accountState, const FolderDefinition& folderDefinition)
{
    auto folder = addFolderInternal(folderDefinition, accountState);

    if(folder) {
        folder->saveToSettings();
        emit folderSyncStateChange(folder);
        emit folderListChanged(_folderMap);
    }
    return folder;
}

Folder* FolderMan::addFolderInternal(FolderDefinition folderDefinition, AccountState* accountState)
{
    auto alias = folderDefinition.alias;
    int count = 0;
    while (folderDefinition.alias.isEmpty() || _folderMap.contains(folderDefinition.alias)) {
        // There is already a folder configured with this name and folder names need to be unique
        folderDefinition.alias = alias + QString::number(++count);
    }

    auto folder = new Folder(folderDefinition, accountState, this );

    if (!ensureJournalGone(folder->journalDbFilePath())) {
        delete folder;
        return 0;
    }

    qDebug() << "Adding folder to Folder Map " << folder << folder->alias();
    _folderMap[folder->alias()] = folder;
    if (folder->syncPaused()) {
        _disabledFolders.insert(folder);
    }

    // See matching disconnects in unloadFolder().
    connect(folder, SIGNAL(scheduleToSync(Folder*)), SLOT(slotScheduleSync(Folder*)));
    connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));
    connect(folder, SIGNAL(syncStateChange()), SLOT(slotForwardFolderSyncStateChange()));
    connect(folder, SIGNAL(syncPausedChanged(Folder*,bool)), SLOT(slotFolderSyncPaused(Folder*,bool)));
    connect(folder, SIGNAL(canSyncChanged()), SLOT(slotFolderCanSyncChanged()));
    connect(&folder->syncEngine().syncFileStatusTracker(), SIGNAL(fileStatusChanged(const QString &, SyncFileStatus)),
            _socketApi.data(), SLOT(slotFileStatusChanged(const QString &, SyncFileStatus)));
    connect(folder, SIGNAL(watchedFileChangedExternally(QString)),
            &folder->syncEngine().syncFileStatusTracker(), SLOT(slotPathTouched(QString)));

    registerFolderMonitor(folder);
    return folder;
}

Folder *FolderMan::folderForPath(const QString &path)
{
    QString absolutePath = QDir::cleanPath(path)+QLatin1Char('/');

    foreach(Folder* folder, this->map().values()) {
        const QString folderPath = folder->cleanPath()+QLatin1Char('/');

        if(absolutePath.startsWith(folderPath)) {
            //qDebug() << "found folder: " << folder->path() << " for " << absolutePath;
            return folder;
        }
    }

    return 0;
}

QStringList FolderMan::findFileInLocalFolders( const QString& relPath, const AccountPtr acc )
{
    QStringList re;

    foreach(Folder* folder, this->map().values()) {
        if (acc != 0 && folder->accountState()->account() != acc) {
            continue;
        }
        QString path = folder->cleanPath();
        QString remRelPath;
        // cut off the remote path from the server path.
        remRelPath = relPath.mid(folder->remotePath().length());
        path += "/";
        path += remRelPath;
        if( QFile::exists(path) ) {
            re.append( path );
        }
    }
    return re;
}

void FolderMan::slotRemoveFolder( Folder *f )
{
    if( !f ) {
        qWarning() << "!! Can not remove null folder";
        return;
    }

    qDebug() << "Removing " << f->alias();

    const bool currentlyRunning = (_currentSyncFolder == f);
    if( currentlyRunning ) {
        // abort the sync now
        terminateSyncProcess();
    }

    if (_scheduleQueue.removeAll(f) > 0) {
        emit scheduleQueueChanged();
    }

    f->wipe();
    f->setSyncPaused(true);

    // remove the folder configuration
    f->removeFromSettings();

    unloadFolder( f);
    if( currentlyRunning ) {
        // We want to schedule the next folder once this is done
        connect(f, SIGNAL(syncFinished(SyncResult)),
                SLOT(slotFolderSyncFinished(SyncResult)));
        // Let the folder delete itself when done.
        connect(f, SIGNAL(syncFinished(SyncResult)), f, SLOT(deleteLater()));
    } else {
        delete f;
    }
}

QString FolderMan::getBackupName( QString fullPathName ) const
{
    if (fullPathName.endsWith("/"))
        fullPathName.chop(1);

    if( fullPathName.isEmpty() ) return QString::null;

     QString newName = fullPathName + tr(" (backup)");
     QFileInfo fi( newName );
     int cnt = 2;
     do {
         if( fi.exists() ) {
             newName = fullPathName + tr(" (backup %1)").arg(cnt++);
             fi.setFile(newName);
         }
     } while( fi.exists() );

     return newName;
}

bool FolderMan::startFromScratch( const QString& localFolder )
{
    if( localFolder.isEmpty() ) {
        return false;
    }

    QFileInfo fi( localFolder );
    QDir parentDir( fi.dir() );
    QString folderName = fi.fileName();

    // Adjust for case where localFolder ends with a /
    if ( fi.isDir() ) {
        folderName = parentDir.dirName();
        parentDir.cdUp();
    }

    if( fi.exists() ) {
        // It exists, but is empty -> just reuse it.
        if( fi.isDir() && fi.dir().count() == 0 ) {
            qDebug() << "startFromScratch: Directory is empty!";
            return true;
        }
        // Disconnect the socket api from the database to avoid that locking of the
        // db file does not allow to move this dir.
        Folder *f = folderForPath(localFolder);
        if(f) {
            if( localFolder.startsWith(f->path()) ) {
                _socketApi->slotUnregisterPath(f->alias());
            }
            f->journalDb()->close();
            f->slotTerminateSync(); // Normally it should not be running, but viel hilft viel
        }

        // Make a backup of the folder/file.
        QString newName = getBackupName( parentDir.absoluteFilePath( folderName ) );
        QString renameError;
        if( !FileSystem::rename( fi.absoluteFilePath(), newName, &renameError ) ) {
            qDebug() << "startFromScratch: Could not rename" << fi.absoluteFilePath()
                     << "to" << newName << "error:" << renameError;
            return false;
        }
    }

    if( !parentDir.mkdir( fi.absoluteFilePath() ) ) {
        qDebug() << "startFromScratch: Could not mkdir" << fi.absoluteFilePath();
        return false;
    }

    return true;
}

void FolderMan::setDirtyProxy(bool value)
{
    foreach( Folder *f, _folderMap.values() ) {
        if(f) {
            f->setProxyDirty(value);

            if (f->accountState() && f->accountState()->account()
                    && f->accountState()->account()->networkAccessManager()) {
                // Need to do this so we do not use the old determined system proxy
                f->accountState()->account()->networkAccessManager()->setProxy(
                            QNetworkProxy(QNetworkProxy::DefaultProxy));
            }
        }
    }
}

void FolderMan::setDirtyNetworkLimits()
{
    foreach( Folder *f, _folderMap.values() ) {
        // set only in busy folders. Otherwise they read the config anyway.
        if(f && f->isBusy()) {
            f->setDirtyNetworkLimits();
        }
    }

}

SyncResult FolderMan::accountStatus(const QList<Folder*> &folders)
{
    SyncResult overallResult(SyncResult::Undefined);

    int cnt = folders.count();

    // if one folder: show the state of the one folder.
    // if more folders:
    // if one of them has an error -> show error
    // if one is paused, but others ok, show ok
    // do not show "problem" in the tray
    //
    if( cnt == 1 ) {
        Folder *folder = folders.at(0);
        if( folder ) {
            if( folder->syncPaused() ) {
                overallResult.setStatus(SyncResult::Paused);
            } else {
                SyncResult::Status syncStatus = folder->syncResult().status();


                switch( syncStatus ) {
                case SyncResult::Undefined:
                    overallResult.setStatus(SyncResult::Error);
                    break;
                case SyncResult::NotYetStarted:
                    overallResult.setStatus( SyncResult::NotYetStarted );
                    break;
                case SyncResult::SyncPrepare:
                    overallResult.setStatus( SyncResult::SyncPrepare );
                    break;
                case SyncResult::SyncRunning:
                    overallResult.setStatus( SyncResult::SyncRunning );
                    break;
                case SyncResult::Problem: // don't show the problem icon in tray.
                case SyncResult::Success:
                    if( overallResult.status() == SyncResult::Undefined )
                        overallResult.setStatus( SyncResult::Success );
                    break;
                case SyncResult::Error:
                    overallResult.setStatus( SyncResult::Error );
                    break;
                case SyncResult::SetupError:
                    if ( overallResult.status() != SyncResult::Error )
                        overallResult.setStatus( SyncResult::SetupError );
                    break;
                case SyncResult::SyncAbortRequested:
                    overallResult.setStatus( SyncResult::SyncAbortRequested);
                    break;
                case SyncResult::Paused:
                    overallResult.setStatus( SyncResult::Paused);
                    break;
                }
            }
        }
    } else {
        int errorsSeen = 0;
        int goodSeen = 0;
        int abortOrPausedSeen = 0;
        int runSeen = 0;
        int various = 0;

        foreach ( Folder *folder, folders ) {
            if( folder->syncPaused() ) {
                abortOrPausedSeen++;
            } else {
                SyncResult folderResult = folder->syncResult();
                SyncResult::Status syncStatus = folderResult.status();

                switch( syncStatus ) {
                case SyncResult::Undefined:
                case SyncResult::NotYetStarted:
                    various++;
                    break;
                case SyncResult::SyncPrepare:
                case SyncResult::SyncRunning:
                    runSeen++;
                    break;
                case SyncResult::Problem: // don't show the problem icon in tray.
                case SyncResult::Success:
                    goodSeen++;
                    break;
                case SyncResult::Error:
                case SyncResult::SetupError:
                    errorsSeen++;
                    break;
                case SyncResult::SyncAbortRequested:
                case SyncResult::Paused:
                    abortOrPausedSeen++;
                    // no default case on purpose, check compiler warnings
                }
            }
        }
        bool set = false;
        if( errorsSeen > 0 ) {
            overallResult.setStatus(SyncResult::Error);
            set = true;
        }
        if( !set && abortOrPausedSeen > 0 && abortOrPausedSeen == cnt ) {
            // only if all folders are paused
            overallResult.setStatus(SyncResult::Paused);
            set = true;
        }
        if( !set && runSeen > 0 ) {
            overallResult.setStatus(SyncResult::SyncRunning);
            set = true;
        }
        if( !set && goodSeen > 0 ) {
            overallResult.setStatus(SyncResult::Success);
            set = true;
        }
    }

    return overallResult;
}

QString FolderMan::statusToString( SyncResult syncStatus, bool paused ) const
{
    QString folderMessage;
    switch( syncStatus.status() ) {
    case SyncResult::Undefined:
        folderMessage = tr( "Undefined State." );
        break;
    case SyncResult::NotYetStarted:
        folderMessage = tr( "Waiting to start syncing." );
        break;
    case SyncResult::SyncPrepare:
        folderMessage = tr( "Preparing for sync." );
        break;
    case SyncResult::SyncRunning:
        folderMessage = tr( "Sync is running." );
        break;
    case SyncResult::Success:
        folderMessage = tr( "Last Sync was successful." );
        break;
    case SyncResult::Error:
        break;
    case SyncResult::Problem:
        folderMessage = tr( "Last Sync was successful, but with warnings on individual files.");
        break;
    case SyncResult::SetupError:
        folderMessage = tr( "Setup Error." );
        break;
    case SyncResult::SyncAbortRequested:
        folderMessage = tr( "User Abort." );
        break;
    case SyncResult::Paused:
        folderMessage = tr("Sync is paused.");
        break;
    // no default case on purpose, check compiler warnings
    }
    if( paused ) {
        // sync is disabled.
        folderMessage = tr( "%1 (Sync is paused)" ).arg(folderMessage);
    }
    return folderMessage;
}

QString FolderMan::checkPathValidityForNewFolder(const QString& path, bool forNewDirectory)
{
    if (path.isEmpty()) {
        return tr("No valid folder selected!");
    }

    QFileInfo selFile( path );
    QString userInput = selFile.canonicalFilePath();

    if (!selFile.exists()) {
        return checkPathValidityForNewFolder(selFile.dir().path(), true);
    }

    if( !selFile.isDir() ) {
        return tr("The selected path is not a folder!");
    }

    if ( !selFile.isWritable() ) {
        return tr("You have no permission to write to the selected folder!");
    }

    // check if the local directory isn't used yet in another ownCloud sync

    for (auto i = _folderMap.constBegin(); i != _folderMap.constEnd(); ++i ) {
        Folder *f = static_cast<Folder*>(i.value());
        QString folderDir = QDir( f->path() ).canonicalPath();
        if( folderDir.isEmpty() ) {
            continue;
        }
        if( ! folderDir.endsWith(QLatin1Char('/')) ) folderDir.append(QLatin1Char('/'));

        if (QDir::cleanPath(f->path()) == QDir::cleanPath(userInput)
                && QDir::cleanPath(QDir(f->path()).canonicalPath()) == QDir(userInput).canonicalPath()) {
            return tr("The local folder %1 is already used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(userInput));
        }
        if (!forNewDirectory && QDir::cleanPath(folderDir).startsWith(QDir::cleanPath(userInput)+'/')) {
            return tr("The local folder %1 already contains a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(userInput));
        }

        QString absCleanUserFolder = QDir::cleanPath(QDir(userInput).canonicalPath())+'/';
        if (!forNewDirectory && QDir::cleanPath(folderDir).startsWith(absCleanUserFolder) ) {
            return tr("The local folder %1 is a symbolic link. "
                      "The link target already contains a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(userInput));
        }

        if (QDir::cleanPath(QString(userInput)).startsWith( QDir::cleanPath(folderDir)+'/')) {
            return tr("The local folder %1 is already contained in a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(userInput));
        }

        if (absCleanUserFolder.startsWith( QDir::cleanPath(folderDir)+'/')) {
            return tr("The local folder %1 is a symbolic link. "
                      "The link target is already contained in a folder used in a folder sync connection. "
                      "Please pick another one!")
                .arg(QDir::toNativeSeparators(userInput));
        }
    }

    return QString();

}

bool FolderMan::ignoreHiddenFiles() const
{
    if (_folderMap.empty()) {
        return true;
    }
    return _folderMap.begin().value()->ignoreHiddenFiles();
}

void FolderMan::setIgnoreHiddenFiles(bool ignore)
{
    // Note that the setting will revert to 'true' if all folders
    // are deleted...
    foreach (Folder* folder, _folderMap) {
        folder->setIgnoreHiddenFiles(ignore);
        folder->saveToSettings();
    }
}

QQueue<Folder*> FolderMan::scheduleQueue() const
{
    return _scheduleQueue;
}

Folder *FolderMan::currentSyncFolder() const
{
    return _currentSyncFolder;
}

void FolderMan::restartApplication()
{
    if( Utility::isLinux() ) {
        // restart:
        qDebug() << "### Restarting application NOW, PID" << qApp->applicationPid() << "is ending.";
        qApp->quit();
        QStringList args = qApp->arguments();
        QString prg = args.takeFirst();

        QProcess::startDetached(prg, args);
    } else {
        qDebug() << "On this platform we do not restart.";
    }
}

} // namespace OCC
