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
#include "accountmigrator.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "filesystem.h"

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include <QMessageBox>
#include <QPointer>
#include <QtCore>
#include <QMutableSetIterator>
#include <QSet>

namespace OCC {

FolderMan* FolderMan::_instance = 0;

/**
 * The minimum time between a sync being requested and it
 * being executed in milliseconds.
 *
 * This delay must be large enough to ensure fileIsStillChanging()
 * in the upload propagator doesn't decide to skip the file because
 * the modification was too recent.
 */
static qint64 msBetweenRequestAndSync = 2000;

FolderMan::FolderMan(QObject *parent) :
    QObject(parent),
    _currentSyncFolder(0),
    _syncEnabled( true )
{
    Q_ASSERT(!_instance);
    _instance = this;

    _socketApi = new SocketApi(this);
    _socketApi->slotReadExcludes();

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

    if( _socketApi ) {
        _socketApi->slotUnregisterPath(f->alias());
    }

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
        ConfigFile cfg;
        fw->addIgnoreListFile( cfg.excludeFile(ConfigFile::SystemScope) );
        fw->addIgnoreListFile( cfg.excludeFile(ConfigFile::UserScope) );

        // Connect the pathChanged signal, which comes with the changed path,
        // to the signal mapper which maps to the folder alias. The changed path
        // is lost this way, but we do not need it for the current implementation.
        connect(fw, SIGNAL(pathChanged(QString)), folder, SLOT(slotWatchedPathChanged(QString)));
        _folderWatchers.insert(folder->alias(), fw);

        // This is at the moment only for the behaviour of the SocketApi.
        connect(fw, SIGNAL(pathChanged(QString)), folder, SLOT(watcherSlot(QString)));
    }

    // register the folder with the socket API
    if( _socketApi ) {
        _socketApi->slotRegisterPath(folder->alias());
    }
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
        return setupFoldersMigration();
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
                Folder* f = addFolderInternal(folderDefinition);
                if (f) {
                    f->setAccountState( account.data() );
                    slotScheduleSync(f);
                    emit folderSyncStateChange(f);
                }
            }
        }
        settings->endGroup(); // Folders
        settings->endGroup(); // <account>
    }

    emit folderListLoaded(_folderMap);

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

    // Normaly there should be only one account when migrating.
    AccountState* accountState = AccountManager::instance()->accounts().value(0).data();
    foreach ( const QString& alias, list ) {
        Folder *f = setupFolderFromOldConfigFile( alias, accountState );
        if( f ) {
            slotScheduleSync(f);
            emit( folderSyncStateChange( f ) );
        }
    }

    emit folderListLoaded(_folderMap);

    // return the number of valid folders.
    return _folderMap.size();
}

bool FolderMan::ensureJournalGone(const QString &localPath)
{
	// FIXME move this to UI, not libowncloudsync
    // remove old .csync_journal file
    QString stateDbFile = localPath+QLatin1String("/.csync_journal.db");
    while (QFile::exists(stateDbFile) && !QFile::remove(stateDbFile)) {
        qDebug() << "Could not remove old db file at" << stateDbFile;
        int ret = QMessageBox::warning(0, tr("Could not reset folder state"),
                                       tr("An old sync journal '%1' was found, "
                                          "but could not be removed. Please make sure "
                                          "that no application is currently using it.")
                                       .arg(QDir::fromNativeSeparators(QDir::cleanPath(stateDbFile))),
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
    return this->_socketApi;
}

QString FolderMan::unescapeAlias( const QString& alias ) const
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
    // check the unescaped variant (for the case the filename comes out
    // of the directory listing. If the file is not existing, escape the
    // file and try again.
    QFileInfo cfgFile( _folderConfigPath, file);

    if( !cfgFile.exists() ) {
        // try the escaped variant.
        escapedAlias = escapeAlias(file);
        cfgFile.setFile( _folderConfigPath, escapedAlias );
    }
    if( !cfgFile.isReadable() ) {
        qDebug() << "Can not read folder definition for alias " << cfgFile.filePath();
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

    folder = addFolderInternal(folderDefinition);
    if (folder) {
        folder->setAccountState(accountState);

        QStringList blackList = settings.value( QLatin1String("blackList")).toStringList();
        if (!blackList.empty()) {
            //migrate settings
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);
            settings.remove(QLatin1String("blackList"));
        }

        folder->saveToSettings();
    }
    return folder;
}

void FolderMan::slotSetFolderPaused( Folder *f, bool paused )
{
    if( !f ) {
        qWarning() << "!! slotSetFolderPaused called with empty folder";
        return;
    }

    slotScheduleSync(f);

    if (!paused) {
        _disabledFolders.remove(f);
    } else {
        _disabledFolders.insert(f);
    }
    f->setSyncPaused(paused);
    emit folderSyncStateChange(f);
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

    if( _socketApi ) {
        // We want the SocketAPI to already now update so that it can show the EVAL icon
        // for files/folders. Only do this when not syncing, else we might get a lot
        // of those notifications.
        _socketApi->slotUpdateFolderView(f);
    }

    qDebug() << "Schedule folder " << alias << " to sync!";

    if( ! _scheduleQueue.contains(f) ) {
        if(f->canSync()) {
            f->prepareToSync();
        } else {
            qDebug() << "Folder is not ready to sync, not scheduled!";
            if( _socketApi ) {
                _socketApi->slotUpdateFolderView(f);
            }
            return;
        }
        _scheduleQueue.enqueue(f);
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
        QString alias;
        foreach(Folder *f, _folderMap) {
            if (f->etagJob()) {
                // Caveat: always grabs the first folder with a job, but we think this is Ok for now and avoids us having a seperate queue.
                _currentEtagJob = f->etagJob();
                alias = f->alias();
                break;
            }
        }
        if (_currentEtagJob.isNull()) {
            qDebug() << "No more remote ETag check jobs to schedule.";
        } else {
            qDebug() << "Scheduling" << alias << "to check remote ETag";
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
        qDebug() << "Account" << accountName << "disconnected, "
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
    }
}

// only enable or disable foldermans will to schedule and do syncs.
// this is not the same as Pause and Resume of folders.
void FolderMan::setSyncEnabled( bool enabled )
{
    if (!_syncEnabled && enabled && !_scheduleQueue.isEmpty()) {
        // We have things in our queue that were waiting the the connection to go back on.
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
    msDelay = qMax(msBetweenRequestAndSync, msDelay);

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
        qDebug() << "Currently folder " << _currentSyncFolder->alias() << " is running, wait for finish!";
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

    // Try to start the top scheduled sync.
    Folder *f = _scheduleQueue.dequeue();
    Q_ASSERT(f);

    // Start syncing this folder!
    if(f->canSync()) {
        _currentSyncFolder = f;

        f->startSync( QStringList() );

        // reread the excludes of the socket api
        // FIXME: the excludes need rework.
        if( _socketApi ) {
            _socketApi->slotClearExcludesList();
            _socketApi->slotReadExcludes();
        }
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

void FolderMan::slotFolderSyncStarted( )
{
    qDebug() << ">===================================== sync started for " << _currentSyncFolder->alias();
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  * This delay is particularly useful to avoid late file change notifications
  * (that we caused ourselves by syncing) from triggering another spurious sync.
  */
void FolderMan::slotFolderSyncFinished( const SyncResult& )
{
    qDebug() << "<===================================== sync finished for " << _currentSyncFolder->alias();

    _lastSyncFolder = _currentSyncFolder;
    _currentSyncFolder = 0;

    startScheduledSyncSoon();
}

Folder* FolderMan::addFolder(AccountState* accountState, const FolderDefinition& folderDefinition)
{
    if (!ensureJournalGone(folderDefinition.localPath)) {
        return 0;
    }

    auto folder = addFolderInternal(folderDefinition);
    if(folder) {
        folder->setAccountState(accountState);
        folder->saveToSettings();
    }
    return folder;
}

Folder* FolderMan::addFolderInternal(const FolderDefinition& folderDefinition)
{
    auto folder = new Folder(folderDefinition, this );

    qDebug() << "Adding folder to Folder Map " << folder;
    _folderMap[folder->alias()] = folder;
    if (folder->syncPaused()) {
        _disabledFolders.insert(folder);
    }

    // See matching disconnects in unloadFolder().
    connect(folder, SIGNAL(scheduleToSync(Folder*)), SLOT(slotScheduleSync(Folder*)));
    connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));
    connect(folder, SIGNAL(syncStateChange()), SLOT(slotForwardFolderSyncStateChange()));

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

void FolderMan::slotRemoveFolder( Folder *f )
{
    if( !f ) {
        qWarning() << "!! Can not remove null folder";
        return;
    }

    qDebug() << "Removing " << f->alias();

    const bool currentlyRunning = (_currentSyncFolder == f);
    if( currentlyRunning ) {
        // let the folder delete itself when done and
        // abort the sync now
        connect(f, SIGNAL(syncFinished(SyncResult)), f, SLOT(deleteLater()));
        terminateSyncProcess();
    }

    _scheduleQueue.removeAll(f);

    f->wipe();
    f->setSyncPaused(true);

    // remove the folder configuration
    f->removeFromSettings();

    unloadFolder( f);
    if( !currentlyRunning ) {
        delete f;
    }
}

QString FolderMan::getBackupName( QString fullPathName ) const
{
    if (fullPathName.endsWith("/"))
        fullPathName.chop(1);

    if( fullPathName.isEmpty() ) return QString::null;

     QString newName = fullPathName + QLatin1String(".oC_bak");
     QFileInfo fi( newName );
     int cnt = 1;
     do {
         if( fi.exists() ) {
             newName = fullPathName + QString( ".oC_bak_%1").arg(cnt++);
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
        if( _socketApi ) {
            Folder *f = folderForPath(localFolder);
            if(f) {
                if( localFolder.startsWith(f->path()) ) {
                    _socketApi->slotUnregisterPath(f->alias());
                }
                f->journalDb()->close();
                f->slotTerminateSync(); // Normaly it should not be running, but viel hilft viel
            }
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
                // Need to do this have us not use the old determined system proxy
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
    // if more folder:
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
        folderMessage = tr( "Waits to start syncing." );
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

} // namespace OCC
