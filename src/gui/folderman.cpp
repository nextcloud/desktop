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
    _syncEnabled( true )
{
    _folderChangeSignalMapper = new QSignalMapper(this);
    connect(_folderChangeSignalMapper, SIGNAL(mapped(const QString &)),
            this, SIGNAL(folderSyncStateChange(const QString &)));

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

    connect(AccountStateManager::instance(), SIGNAL(accountStateRemoved(AccountState*)),
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

void FolderMan::unloadFolder( const QString& alias )
{
    Folder* f = folder(alias);
    if( !f ) {
        return;
    }

    if( _socketApi ) {
        _socketApi->slotUnregisterPath(alias);
    }

    if( _folderWatchers.contains(alias)) {
        _folderWatchers.remove(alias);
    }
    _folderMap.remove( alias );
}

int FolderMan::unloadAndDeleteAllFolders()
{
    int cnt = 0;

    // clear the list of existing folders.
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder* f = i.value();
        unloadFolder(i.key());
        delete f;
        cnt++;
    }
    _lastSyncFolder.clear();
    _currentSyncFolder.clear();
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
  qDebug() << "* Setup folders from " << _folderConfigPath;

  unloadAndDeleteAllFolders();

  ConfigFile cfg;
  QDir storageDir(cfg.configPath());
  storageDir.mkpath(QLatin1String("folders"));
  _folderConfigPath = cfg.configPath() + QLatin1String("folders");

  QDir dir( _folderConfigPath );
  //We need to include hidden files just in case the alias starts with '.'
  dir.setFilter(QDir::Files | QDir::Hidden);
  QStringList list = dir.entryList();

  if( list.count() == 0 ) {
      // maybe the account was just migrated.
      AccountPtr acc = AccountManager::instance()->account();
      if ( acc && acc->wasMigrated() ) {
          AccountMigrator accMig;
          list = accMig.migrateFolderDefinitons();
      }
  }

  foreach ( const QString& alias, list ) {
    Folder *f = setupFolderFromConfigFile( alias );
    if( f ) {
        slotScheduleSync(alias);
        emit( folderSyncStateChange( f->alias() ) );
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
Folder* FolderMan::setupFolderFromConfigFile(const QString &file) {
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
    QStringList blackList = settings.value( QLatin1String("blackList")).toStringList();
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

    AccountState* accountState = AccountStateManager::instance()->accountState();
    if (!accountState) {
        qWarning() << "can't create folder without an account";
        return 0;
    }

    folder = new Folder( accountState, alias, path, targetPath, this );
    folder->setConfigFile(cfgFile.absoluteFilePath());
    folder->setSelectiveSyncBlackList(blackList);
    qDebug() << "Adding folder to Folder Map " << folder;
    _folderMap[alias] = folder;
    if (paused) {
        folder->setSyncPaused(paused);
        _disabledFolders.insert(folder);
    }

    /* Use a signal mapper to connect the signals to the alias */
    connect(folder, SIGNAL(scheduleToSync(const QString&)), SLOT(slotScheduleSync(const QString&)));
    connect(folder, SIGNAL(syncStateChange()), _folderChangeSignalMapper, SLOT(map()));
    connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));

    _folderChangeSignalMapper->setMapping( folder, folder->alias() );

    registerFolderMonitor(folder);
    return folder;
}

void FolderMan::slotSetFolderPaused( const QString& alias, bool paused )
{
    Folder *f = folder(alias);
    if( !f ) {
        qDebug() << "!! Can not enable alias " << alias << ", can not be found in folderMap.";
        return;
    }

    slotScheduleSync(alias);

    // FIXME: Use ConfigFile
    QSettings settings(f->configFile(), QSettings::IniFormat);
    settings.beginGroup(escapeAlias(f->alias()));
    if (!paused) {
        settings.remove("paused");
        _disabledFolders.remove(f);
    } else {
        settings.setValue("paused", true);
        _disabledFolders.insert(f);
    }
    emit folderSyncStateChange(alias);
}

// this really terminates the current sync process
// ie. no questions, no prisoners
// csync still remains in a stable state, regardless of that.
void FolderMan::terminateSyncProcess()
{
    Folder *f = folder(_currentSyncFolder);
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

SyncResult FolderMan::syncResult( const QString& alias )
{
    Folder *f = folder( alias );
    return f ? f->syncResult() : SyncResult();
}

void FolderMan::slotScheduleAllFolders()
{
    foreach( Folder *f, _folderMap.values() ) {
        if (f && ! f->syncPaused()) {
            slotScheduleSync( f->alias() );
        }
    }
}

/*
  * if a folder wants to be synced, it calls this slot and is added
  * to the queue. The slot to actually start a sync is called afterwards.
  */
void FolderMan::slotScheduleSync( const QString& alias )
{
    Folder* f = folder(alias);
    if( !f ) {
        qDebug() << "Not scheduling sync for empty or unknown folder" << alias;
        return;
    }

    if( _socketApi ) {
        // We want the SocketAPI to already now update so that it can show the EVAL icon
        // for files/folders. Only do this when not syncing, else we might get a lot
        // of those notifications.
        _socketApi->slotUpdateFolderView(alias);
    }

    qDebug() << "Schedule folder " << alias << " to sync!";

    if( ! _scheduleQueue.contains(alias) ) {
        if( !f->syncPaused() ) {
            f->prepareToSync();
        } else {
            qDebug() << "Folder is not enabled, not scheduled!";
            if( _socketApi ) {
                _socketApi->slotUpdateFolderView(f->alias());
            }
            return;
        }
        _scheduleQueue.enqueue(alias);
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
    emit( folderSyncStateChange(QString::null) );
}

void FolderMan::startScheduledSyncSoon(qint64 msMinimumDelay)
{
    if (_startScheduledSyncTimer.isActive()) {
        return;
    }
    if (_scheduleQueue.empty()) {
        return;
    }
    if (! _currentSyncFolder.isEmpty()) {
        return;
    }

    qint64 msDelay = msMinimumDelay;
    qint64 msSinceLastSync = 0;

    // Require a pause based on the duration of the last sync run.
    if (Folder* lastFolder = folder(_lastSyncFolder)) {
        msSinceLastSync = lastFolder->msecSinceLastSync();

        //  1s   -> 1.5s pause
        // 10s   -> 5s pause
        //  1min -> 12s pause
        //  1h   -> 90s pause
        qint64 pause = qSqrt(lastFolder->msecLastSyncDuration()) / 20.0 * 1000.0;
        msDelay = qMax(msDelay, pause);
    }

    // Punish consecutive follow-up syncs with longer delays.
    if (Folder* nextFolder = folder(_scheduleQueue.head())) {
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
    if( !_currentSyncFolder.isEmpty() ) {
        qDebug() << "Currently folder " << _currentSyncFolder << " is running, wait for finish!";
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
    const QString alias = _scheduleQueue.dequeue();
    Folder *f = folder(alias);
    if( !f ) {
        qDebug() << "FolderMan: Not syncing queued folder" << alias << ": not in folder map anymore";
        return;
    }

    // Start syncing this folder!
    if( !f->syncPaused() ) {
        _currentSyncFolder = alias;

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

    QSet<QString> folderAliases = _folderMap.keys().toSet();
    QMutableSetIterator<QString> i(folderAliases);
    while (i.hasNext()) {
        QString alias = i.next();
        if (_currentSyncFolder == alias) {
            i.remove();
            continue;
        }
        if (_scheduleQueue.contains(alias)) {
            i.remove();
            continue;
        }
        Folder *f = _folderMap.value(alias);
        if (f && _disabledFolders.contains(f)) {
            i.remove();
            continue;
        }
        if (f && (f->etagJob() || f->isBusy() || f->syncPaused())) {
            i.remove();
            continue;
        }
        if (f && f->msecSinceLastSync() < polltime) {
            i.remove();
            continue;
        }
    }

    if (folderAliases.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No folders need to check for the remote ETag";
    } else {
        qDebug() << Q_FUNC_INFO << "The following folders need to check for the remote ETag:" << folderAliases;
        i = folderAliases; // reset
         while (i.hasNext()) {
             QString alias = i.next();
             QMetaObject::invokeMethod(_folderMap.value(alias), "slotRunEtagJob", Qt::QueuedConnection);
         }
    }
}

void FolderMan::slotRemoveFoldersForAccount(AccountState* accountState)
{
    QStringList foldersToRemove;
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder* folder = i.value();
        if (folder->accountState() == accountState) {
            foldersToRemove.append(folder->alias());
        }
    }

    qDebug() << "Account was removed, removing associated folders:" << foldersToRemove;
    foreach (const QString& alias, foldersToRemove) {
        slotRemoveFolder(alias);
    }
}

void FolderMan::slotFolderSyncStarted( )
{
    qDebug() << ">===================================== sync started for " << _currentSyncFolder;
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  * This delay is particularly useful to avoid late file change notifications
  * (that we caused ourselves by syncing) from triggering another spurious sync.
  */
void FolderMan::slotFolderSyncFinished( const SyncResult& )
{
    qDebug() << "<===================================== sync finished for " << _currentSyncFolder;

    _lastSyncFolder = _currentSyncFolder;
    _currentSyncFolder.clear();

    startScheduledSyncSoon();
}

bool FolderMan::addFolderDefinition(const QString& alias, const QString& sourceFolder,
                                    const QString& targetPath, const QStringList& selectiveSyncBlackList)
{
    if (! ensureJournalGone(sourceFolder))
        return false;

    QString escapedAlias = escapeAlias(alias);
    // Create a settings file named after the alias
    QSettings settings( _folderConfigPath + QLatin1Char('/') + escapedAlias, QSettings::IniFormat);
    settings.beginGroup(escapedAlias);
    settings.setValue(QLatin1String("localPath"),   sourceFolder );
    settings.setValue(QLatin1String("targetPath"),  targetPath );
    // for compat reasons
    settings.setValue(QLatin1String("backend"),     "owncloud" );
    settings.setValue(QLatin1String("connection"),  Theme::instance()->appName());
    settings.setValue(QLatin1String("blackList"), selectiveSyncBlackList);
    settings.sync();

    return true;
}

Folder *FolderMan::folderForPath(const QString &path)
{
    QString absolutePath = QDir::cleanPath(path)+QLatin1Char('/');

    foreach(Folder* folder, this->map().values()) {
        const QString folderPath = QDir::cleanPath(folder->path())+QLatin1Char('/');

        if(absolutePath.startsWith(folderPath)) {
            //qDebug() << "found folder: " << folder->path() << " for " << absolutePath;
            return folder;
        }
    }

    return 0;
}

void FolderMan::removeAllFolderDefinitions()
{
    foreach( Folder *f, _folderMap.values() ) {
        if(f) {
            slotRemoveFolder( f->alias() );
        }
    }
    // clear the queue.
    _scheduleQueue.clear();

}

void FolderMan::slotRemoveFolder( const QString& alias )
{
    Folder *f = folder(alias);
    if( !f ) {
        qDebug() << "!! Can not remove " << alias << ", not in folderMap.";
        return;
    }

    qDebug() << "Removing " << alias;

    const bool currentlyRunning = (_currentSyncFolder == alias);
    if( currentlyRunning ) {
        // let the folder delete itself when done and
        // abort the sync now
        connect(f, SIGNAL(syncFinished(SyncResult)), f, SLOT(deleteLater()));
        terminateSyncProcess();
    }

    _scheduleQueue.removeAll(alias);

    f->wipe();
    f->setSyncPaused(true);

    // remove the folder configuration
    QFile file(f->configFile() );
    if( file.exists() ) {
        qDebug() << "Remove folder config file " << file.fileName();
        file.remove();
    }

    unloadFolder( alias );
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
        int abortSeen = 0;
        int runSeen = 0;
        int various = 0;

        foreach ( Folder *folder, folders ) {
            if( folder->syncPaused() ) {
                abortSeen++;
            } else {
                SyncResult folderResult = folder->syncResult();
                SyncResult::Status syncStatus = folderResult.status();

                switch( syncStatus ) {
                case SyncResult::Undefined:
                case SyncResult::NotYetStarted:
                case SyncResult::SyncPrepare:
                    various++;
                    break;
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
                    abortSeen++;
                    // no default case on purpose, check compiler warnings
                }
            }
        }
        bool set = false;
        if( errorsSeen > 0 ) {
            overallResult.setStatus(SyncResult::Error);
            set = true;
        }
        if( !set && abortSeen > 0 && abortSeen == cnt ) {
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
