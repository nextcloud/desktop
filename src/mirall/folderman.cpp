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

#include "mirall/folderman.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/folder.h"
#include "mirall/syncresult.h"
#include "mirall/inotify.h"
#include "mirall/theme.h"

#include <neon/ne_socket.h>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include <QDesktopServices>
#include <QMessageBox>
#include <QtCore>

namespace Mirall {

FolderMan* FolderMan::_instance = 0;

FolderMan::FolderMan(QObject *parent) :
    QObject(parent),
    _syncEnabled( true )
{
    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    MirallConfigFile cfg;
    QDir storageDir(cfg.configPath());
    storageDir.mkpath(QLatin1String("folders"));
    _folderConfigPath = cfg.configPath() + QLatin1String("folders");

    _folderChangeSignalMapper = new QSignalMapper(this);
    connect(_folderChangeSignalMapper, SIGNAL(mapped(const QString &)),
            this, SIGNAL(folderSyncStateChange(const QString &)));
}

FolderMan *FolderMan::instance()
{
    if(!_instance) {
        _instance = new FolderMan;
        ne_sock_init();
    }

    return _instance;
}

FolderMan::~FolderMan()
{
    qDeleteAll(_folderMap);
    ne_sock_exit();
}

Mirall::Folder::Map FolderMan::map()
{
    return _folderMap;
}

int FolderMan::unloadAllFolders()
{
    int cnt = 0;

    // clear the list of existing folders.
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        delete _folderMap.take( i.key() );
        cnt++;
    }
    _currentSyncFolder.clear();
    return cnt;
}

int FolderMan::setupFolders()
{
  qDebug() << "* Setup folders from " << _folderConfigPath;

  unloadAllFolders();

  QDir dir( _folderConfigPath );
  dir.setFilter(QDir::Files);
  QStringList list = dir.entryList();

  foreach ( const QString& alias, list ) {
    Folder *f = setupFolderFromConfigFile( alias );
    if( f ) {
        emit( folderSyncStateChange( f->alias() ) );
    }
  }
  // return the number of valid folders.
  return _folderMap.size();
}

void FolderMan::wipeAllJournals()
{
    terminateCurrentSync();

    foreach( Folder *f, _folderMap.values() ) {
        f->wipe();
    }
}

bool FolderMan::ensureJournalGone(const QString &localPath)
{

    // remove old .csync_journal file
    QString stateDbFile = localPath+QLatin1String("/.csync_journal.db");
    while (QFile::exists(stateDbFile) && !QFile::remove(stateDbFile)) {
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

void FolderMan::terminateCurrentSync()
{
    if( !_currentSyncFolder.isEmpty() ) {
        qDebug() << "Terminating syncing on folder " << _currentSyncFolder;
        terminateSyncProcess( _currentSyncFolder );
    }
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

QString FolderMan::escapeAlias( const QString& alias ) const
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

    folder = new Folder( alias, path, targetPath, this );
    folder->setConfigFile(file);
    qDebug() << "Adding folder to Folder Map " << folder;
    _folderMap[alias] = folder;
    if (paused) {
        _disabledFolders.insert(folder);
    }

    /* Use a signal mapper to connect the signals to the alias */
    connect(folder, SIGNAL(scheduleToSync(const QString&)), SLOT(slotScheduleSync(const QString&)));
    connect(folder, SIGNAL(syncStateChange()), _folderChangeSignalMapper, SLOT(map()));
    connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));

    _folderChangeSignalMapper->setMapping( folder, folder->alias() );
    return folder;
}

void FolderMan::slotEnableFolder( const QString& alias, bool enable )
{
    if( ! _folderMap.contains( alias ) ) {
      qDebug() << "!! Can not enable alias " << alias << ", can not be found in folderMap.";
      return;
    }

    Folder *f = _folderMap[alias];
    if( f ) {
        f->setSyncEnabled(enable);
        f->evaluateSync(QStringList());

        QSettings settings(_folderConfigPath + QLatin1Char('/') + f->configFile(), QSettings::IniFormat);
        settings.beginGroup(escapeAlias(f->alias()));
        if (enable) {
            settings.remove("paused");
            _disabledFolders.remove(f);
        } else {
            settings.setValue("paused", true);
            _disabledFolders.insert(f);
        }
    }
}

// this really terminates, ie. no questions, no prisoners.
// csync still remains in a stable state, regardless of that.
void FolderMan::terminateSyncProcess( const QString& alias )
{
    QString folderAlias = alias;
    if( alias.isEmpty() ) {
        folderAlias = _currentSyncFolder;
    }
    if( ! folderAlias.isEmpty() ) {
        Folder *f = _folderMap[folderAlias];
        if( f ) {
            f->slotTerminateSync(true);
            if(_currentSyncFolder == folderAlias )
                _currentSyncFolder.clear();
        }
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
    return syncResult(f);
}

SyncResult FolderMan::syncResult( Folder *f )
{
   return f ? f->syncResult() : SyncResult();
}

void FolderMan::slotScheduleAllFolders()
{
    foreach( Folder *f, _folderMap.values() ) {
        if (f->syncEnabled()) {
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
    if( alias.isEmpty() ) return;

    qDebug() << "Schedule folder " << alias << " to sync!";
    if( _currentSyncFolder == alias ) {
        qDebug() << " the current folder is currently syncing.";
        return;
    }

    if( ! _scheduleQueue.contains(alias )) {
        _scheduleQueue.enqueue(alias);
    } else {
        qDebug() << " II> Sync for folder " << alias << " already scheduled, do not enqueue!";
    }
    slotScheduleFolderSync();
}

void FolderMan::setSyncEnabled( bool enabled )
{
    if (!_syncEnabled && enabled && !_scheduleQueue.isEmpty()) {
        // We have things in our queue that were waiting the the connection to go back on.
        QTimer::singleShot(200, this, SLOT(slotScheduleFolderSync()));
    }
    _syncEnabled = enabled;

    foreach( Folder *f, _folderMap.values() ) {
        f->setSyncEnabled(enabled && !_disabledFolders.contains(f));
    }
}

/*
  * slot to start folder syncs.
  * It is either called from the slot where folders enqueue themselves for
  * syncing or after a folder sync was finished.
  */
void FolderMan::slotScheduleFolderSync()
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
    if( ! _scheduleQueue.isEmpty() ) {
        const QString alias = _scheduleQueue.dequeue();
        if( _folderMap.contains( alias ) ) {
            Folder *f = _folderMap[alias];
            if( f->syncEnabled() ) {
                _currentSyncFolder = alias;
                f->startSync( QStringList() );
            }
        }
    }
}

void FolderMan::slotFolderSyncStarted( )
{
    qDebug() << ">===================================== sync started for " << _currentSyncFolder;
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  */
void FolderMan::slotFolderSyncFinished( const SyncResult& )
{
    qDebug() << "<===================================== sync finished for " << _currentSyncFolder;

    _currentSyncFolder.clear();
    QTimer::singleShot(200, this, SLOT(slotScheduleFolderSync()));
}

void FolderMan::addFolderDefinition(const QString& alias, const QString& sourceFolder, const QString& targetPath )
{
    QString escapedAlias = escapeAlias(alias);
    // Create a settings file named after the alias
    QSettings settings( _folderConfigPath + QLatin1Char('/') + escapedAlias, QSettings::IniFormat);
    settings.beginGroup(escapedAlias);
    settings.setValue(QLatin1String("localPath"),   sourceFolder );
    settings.setValue(QLatin1String("targetPath"),  targetPath );
    // for compat reasons
    settings.setValue(QLatin1String("backend"),     "owncloud" );
    settings.setValue(QLatin1String("connection"),  Theme::instance()->appName());
    settings.sync();
}

Folder *FolderMan::folderForPath(const QUrl &path)
{
    QString absolutePath = path.toLocalFile();
    absolutePath.append("/");

    foreach(Folder* folder, map().values())
    {
        if(absolutePath.startsWith(folder->path()))
        {
            qDebug() << "found folder: " << folder->path() << " for " << absolutePath;
            return folder;
        }
    }

    return 0;
}

void FolderMan::removeAllFolderDefinitions()
{
    foreach( Folder *f, _folderMap.values() ) {
        slotRemoveFolder( f->alias() );
    }
    // clear the queue.
    _scheduleQueue.clear();

}

void FolderMan::slotRemoveFolder( const QString& alias )
{
    if( alias.isEmpty() ) return;

    if( _currentSyncFolder == alias ) {
        // terminate if the sync is currently underway.
        terminateSyncProcess( alias );
    }
    removeFolder(alias);
}

// remove a folder from the map. Should be sure n
void FolderMan::removeFolder( const QString& alias )
{
    Folder *f = 0;

    _scheduleQueue.removeAll(alias);

    if( _folderMap.contains( alias )) {
        qDebug() << "Removing " << alias;
        f = _folderMap.take( alias );
        f->wipe();
    } else {
        qDebug() << "!! Can not remove " << alias << ", not in folderMap.";
    }

    if( f ) {
        QFile file( _folderConfigPath + QLatin1Char('/') + f->configFile() );
        if( file.exists() ) {
            qDebug() << "Remove folder config file " << file.fileName();
            file.remove();
        }
        // FIXME: this is a temporar dirty fix against a crash happening because
        // the csync owncloud module still has static components. Activate the
        // delete once the module is fixed.
        // f->deleteLater();
    }
}

QString FolderMan::getBackupName( const QString& fullPathName ) const
{
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
    if( localFolder.isEmpty() ) return false;

    QFileInfo fi( localFolder );
    if( fi.exists() && fi.isDir() ) {
        QDir file = fi.dir();

        // check if there are files in the directory.
        if( file.count() == 0 ) {
            // directory is existing, but its empty. Use it.
            qDebug() << "startFromScratch: Directory is empty!";
            return true;
        }
        QString newName = getBackupName( fi.absoluteFilePath() );

        if( file.rename( fi.absoluteFilePath(), newName )) {
            if( file.mkdir( fi.absoluteFilePath() ) ) {
                return true;
            }
        }
    }
    return false;
}

void FolderMan::setDirtyProxy(bool value)
{
    foreach( Folder *f, _folderMap.values() ) {
        f->setProxyDirty(value);
    }
}


SyncResult FolderMan::accountStatus(const QList<Folder*> &folders)
{
    SyncResult overallResult(SyncResult::Undefined);

    foreach ( Folder *folder, folders ) {
        SyncResult folderResult = folder->syncResult();
        SyncResult::Status syncStatus = folderResult.status();

        switch( syncStatus ) {
        case SyncResult::Undefined:
            if ( overallResult.status() != SyncResult::Error )
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
        case SyncResult::Unavailable:
            overallResult.setStatus( SyncResult::Unavailable );
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
            break;
            // no default case on purpose, check compiler warnings
        }
    }
    return overallResult;
}

QString FolderMan::statusToString( SyncResult syncStatus, bool enabled ) const
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
    case SyncResult::Unavailable:
        folderMessage = tr( "Server is currently not available." );
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
    // no default case on purpose, check compiler warnings
    }
    if( !enabled ) {
        // sync is disabled.
        folderMessage = tr( "%1 (Sync is paused)" ).arg(folderMessage);
    }
    return folderMessage;
}

} // namespace Mirall
