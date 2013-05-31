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
#include "mirall/owncloudfolder.h"
#include "mirall/syncresult.h"
#include "mirall/inotify.h"
#include "mirall/theme.h"

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include <QDesktopServices>
#include <QtCore>

namespace Mirall {

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

FolderMan::~FolderMan()
{
    foreach (Folder *folder, _folderMap) {
        delete folder;
    }
}

Mirall::Folder::Map FolderMan::map()
{
    return _folderMap;
}


int FolderMan::setupFolders()
{
    // setup a handler to look for configuration changes
#ifdef CHECK_FOR_SETUP_CHANGES
    _configFolderWatcher = new FolderWatcher( _folderConfigPath );
    _configFolderWatcher->setEventInterval(20000);
    connect(_configFolderWatcher, SIGNAL(folderChanged(const QStringList &)),
            this, SLOT( slotReparseConfiguration()) );
#endif
    int cnt = setupKnownFolders();

    return cnt;
}

void FolderMan::slotReparseConfiguration()
{
    setupKnownFolders();
}


int FolderMan::setupKnownFolders()
{
  qDebug() << "* Setup folders from " << _folderConfigPath;

  // first terminate sync jobs.
  terminateCurrentSync();

  // clear the list of existing folders.
  Folder::MapIterator i(_folderMap);
  while (i.hasNext()) {
      i.next();
      delete _folderMap.take( i.key() );
  }

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

    QSettings settings( cfgFile.filePath(), QSettings::IniFormat);
    qDebug() << "    -> file path: " << settings.fileName();

    // Check if the filename is equal to the group setting. If not, use the group
    // name as an alias.
    QStringList groups = settings.childGroups();

    if( ! groups.contains(escapedAlias) && groups.count() > 0 ) {
        escapedAlias = groups.first();
    }

    settings.beginGroup( escapedAlias ); // read the group with the same name as the file which is the folder alias

    QString path = settings.value(QLatin1String("localpath")).toString();
    QString backend = settings.value(QLatin1String("backend")).toString();
    QString targetPath = settings.value( QLatin1String("targetPath") ).toString();
    // QString connection = settings.value( QLatin1String("connection") ).toString();
    QString alias = unescapeAlias( escapedAlias );

    if (!backend.isEmpty()) {

        if( backend == QLatin1String("owncloud") ) {
            MirallConfigFile cfgFile;

            // assemble the owncloud url to pass to csync, incl. webdav
            QString oCUrl = cfgFile.ownCloudUrl( QString::null, true );

            // cut off the leading slash, oCUrl always has a trailing.
            if( targetPath.startsWith(QLatin1Char('/')) ) {
                targetPath.remove(0,1);
            }

            folder = new ownCloudFolder( alias, path, oCUrl + targetPath, this );
            folder->setConfigFile(file);
        } else {
            qWarning() << "unknown backend" << backend;
            return NULL;
        }
    }

    if( folder ) {
        folder->setBackend( backend );
        // folder->setOnlyOnlineEnabled(settings.value("folder/onlyOnline", false).toBool());
        folder->setOnlyThisLANEnabled(settings.value(QLatin1String("folder/onlyThisLAN"), false).toBool());

        _folderMap[alias] = folder;

        qDebug() << "Adding folder to Folder Map " << folder;
        /* Use a signal mapper to connect the signals to the alias */
        connect(folder, SIGNAL(scheduleToSync(const QString&)), SLOT(slotScheduleSync(const QString&)));
        connect(folder, SIGNAL(syncStateChange()), _folderChangeSignalMapper, SLOT(map()));
        connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
        connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));

        _folderChangeSignalMapper->setMapping( folder, folder->alias() );
    }
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
    }
}

// this really terminates, ie. no questions, no prisoners.
// csync still remains in a stable state, regardless of that.
void FolderMan::terminateSyncProcess( const QString& alias )
{
    Folder *f = _folderMap[alias];
    if( f ) {
        f->slotTerminateSync();

        if(_currentSyncFolder == alias )
            _currentSyncFolder = QString::null;
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
    SyncResult res;
    Folder *f = folder( alias );

    if( f ) {
        res = f->syncResult();
    }
    return res;
}

void FolderMan::slotScheduleAllFolders()
{
    foreach( Folder *f, _folderMap.values() ) {
        slotScheduleSync( f->alias() );
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
        // the current folder is currently syncing.
        return;
    }

    if( ! _scheduleQueue.contains(alias )) {
        _scheduleQueue.append(alias);
    } else {
        qDebug() << " II> Sync for folder " << alias << " already scheduled, do not enqueue!";
    }

    slotScheduleFolderSync();

}

void FolderMan::setSyncEnabled( bool enabled )
{
    _syncEnabled = enabled;
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
        const QString alias = _scheduleQueue.takeFirst();
        if( _folderMap.contains( alias ) ) {
            Folder *f = _folderMap[alias];
            _currentSyncFolder = alias;
            f->startSync( QStringList() );
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

/**
  * Add a folder definition to the config
  * Params:
  * QString backend
  * QString alias
  * QString sourceFolder on local machine
  * QString targetPath on remote
  * bool    onlyThisLAN, currently unused.
  */
void FolderMan::addFolderDefinition( const QString& backend, const QString& alias,
                                     const QString& sourceFolder, const QString& targetPath,
                                     bool onlyThisLAN )
{
    QString escapedAlias = escapeAlias(alias);
    // Create a settings file named after the alias
    QSettings settings( _folderConfigPath + QLatin1Char('/') + escapedAlias, QSettings::IniFormat);

    settings.setValue(QString::fromLatin1("%1/localPath").arg(escapedAlias),   sourceFolder );
    settings.setValue(QString::fromLatin1("%1/targetPath").arg(escapedAlias),  targetPath );
    settings.setValue(QString::fromLatin1("%1/backend").arg(escapedAlias),     backend );
    settings.setValue(QString::fromLatin1("%1/connection").arg(escapedAlias),  Theme::instance()->appName());
    settings.setValue(QString::fromLatin1("%1/onlyThisLAN").arg(escapedAlias), onlyThisLAN );
    settings.sync();
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
        f->deleteLater();
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
             newName += fullPathName + QString( ".oC_bak_%1").arg(cnt++);
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
        QString newName = getBackupName( fi.absoluteFilePath() );

        if( file.rename( fi.absoluteFilePath(), newName )) {
            if( file.mkdir( fi.absoluteFilePath() ) ) {
                return true;
            }
        }
    }
    return false;
}

void FolderMan::setProxy()
{
    foreach( Folder *f, _folderMap.values() ) {
        f->setProxy();
    }
}

}
