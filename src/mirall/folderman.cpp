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

#include <QDesktopServices>
#include <QtCore>

#include "mirall/mirallconfigfile.h"
#include "mirall/unisonfolder.h"
#include "mirall/csyncfolder.h"
#include "mirall/owncloudfolder.h"
#include "mirall/syncresult.h"
#include "mirall/folderman.h"
#include "mirall/inotify.h"

namespace Mirall {

FolderMan::FolderMan(QObject *parent) :
    QObject(parent),
    _folderToDelete(false)
{
    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    storageDir.mkpath("folders");
    _folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

#ifdef USE_WATCHER
    Mirall::INotify::initialize();
#endif

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

    // do an initial sync
    foreach( Folder *f, _folderMap.values() ) {
    //    f->slotChanged();
    }
    return cnt;
}

void FolderMan::slotReparseConfiguration()
{
    setupKnownFolders();
}


int FolderMan::setupKnownFolders()
{
  qDebug() << "* Setup folders from " << _folderConfigPath;

  _folderMap.clear(); // FIXME: check if delete of folder structure happens

  QDir dir( _folderConfigPath );
  dir.setFilter(QDir::Files);
  QStringList list = dir.entryList();

  foreach ( QString alias, list ) {
    Folder *f = setupFolderFromConfigFile( alias );
  }
  // return the number of valid folders.
  return _folderMap.size();
}

// filename is the name of the file only, it does not include
// the configuration directory path
Folder* FolderMan::setupFolderFromConfigFile(const QString &file) {
    Folder *folder = 0L;

    qDebug() << "  ` -> setting up:" << file;
    QSettings settings( _folderConfigPath + "/" + file, QSettings::IniFormat);
    qDebug() << "    -> file path: " + settings.fileName();

    settings.beginGroup( file ); // read the group with the same name as the file which is the folder alias

    QString path = settings.value("localpath").toString();
    if ( path.isNull() || !QFileInfo( path ).isDir() ) {
        qWarning() << "    `->" << path << "does not exist. Skipping folder" << file;
        // _tray->showMessage(tr("Unknown folder"),
        //                    tr("Folder %1 does not exist").arg(path.toString()),
        //                    QSystemTrayIcon::Critical);
        return folder;
    }

    QString backend = settings.value("backend").toString();
    QString targetPath = settings.value( "targetPath" ).toString();
    QString connection = settings.value( "connection" ).toString();

    if (!backend.isEmpty()) {

        if (backend == "unison") {
            folder = new UnisonFolder(file, path, targetPath, this );
        } else if (backend == "csync") {
#ifdef WITH_CSYNC
            folder = new CSyncFolder(file, path, targetPath, this );
#else
            qCritical() << "* csync support not enabled!! ignoring:" << file;
#endif
        } else if( backend == "owncloud" ) {
#ifdef WITH_CSYNC

            MirallConfigFile cfgFile;

            // assemble the owncloud url to pass to csync.
            QUrl url( cfgFile.ownCloudUrl() );
            QString existPath = url.path();
            qDebug() << "existing path: "  << existPath;

            if( !existPath.isEmpty() ) {
                // cut off the trailing slash
                if( existPath.endsWith('/') ) {
                    existPath.truncate( existPath.length()-1 );
                }
                // cut off the leading slash
                if( targetPath.startsWith('/') ) {
                    targetPath.remove(0,1);
                }
            }

            url.setPath( QString("%1/files/webdav.php/%2").arg(existPath).arg(targetPath) );

            folder = new ownCloudFolder( file, path, url.toString(), this );


#else
            qCritical() << "* owncloud support not enabled!! ignoring:" << file;
#endif
        } else {
            qWarning() << "unknown backend" << backend;
            return NULL;
        }
    }
    folder->setBackend( backend );
    // folder->setOnlyOnlineEnabled(settings.value("folder/onlyOnline", false).toBool());
    folder->setOnlyThisLANEnabled(settings.value("folder/onlyThisLAN", false).toBool());

    _folderMap[file] = folder;

    qDebug() << "Adding folder to Folder Map " << folder;
    /* Use a signal mapper to connect the signals to the alias */
    connect(folder, SIGNAL(scheduleToSync(const QString&)), SLOT(slotScheduleSync(const QString&)));
    connect(folder, SIGNAL(syncStateChange()), _folderChangeSignalMapper, SLOT(map()));
    connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    connect(folder, SIGNAL(syncFinished(SyncResult)), SLOT(slotFolderSyncFinished(SyncResult)));

    _folderChangeSignalMapper->setMapping( folder, folder->alias() );

    return folder;
}

void FolderMan::disableFoldersWithRestore()
{
  _folderEnabledMap.clear();
  foreach( Folder *f, _folderMap ) {
    // store the enabled state, then make sure it is disabled
    _folderEnabledMap.insert(f->alias(), f->syncEnabled());
    f->setSyncEnabled(false);
  }
}

void FolderMan::restoreEnabledFolders()
{
  foreach( Folder *f, _folderMap ) {
    if (_folderEnabledMap.contains( f->alias() )) {
        f->setSyncEnabled( _folderEnabledMap.value( f->alias() ) );
    }
  }
}

void FolderMan::slotEnableFolder( const QString& alias, bool enable )
{
    if( ! _folderMap.contains( alias ) ) {
      qDebug() << "!! Can not enable alias " << alias << ", can not be found in folderMap.";
      return;
    }

    Folder *f = _folderMap[alias];
    f->setSyncEnabled(enable);
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
    }

    if( _scheduleQueue.contains( alias ) ) {
        qDebug() << " II> Sync for folder " << alias << " already scheduled, do not enqueue!";
    } else {
        _scheduleQueue.append( alias );

        slotScheduleFolderSync();
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
    qDebug() << "<===================================== sync finsihed for " << _currentSyncFolder;

    // check if the folder is scheduled to be deleted. The flag is set in slotRemoveFolder
    // after the user clicked to delete it.
    if( _folderToDelete ) {
        qDebug() << " !! This folder is going to be deleted now!";
        removeFolder( _currentSyncFolder );
        _folderToDelete = false;
    }
    _currentSyncFolder = QString();
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
    // Create a settings file named after the alias
    QSettings settings( _folderConfigPath + "/" + alias, QSettings::IniFormat);

    settings.setValue(QString("%1/localPath").arg(alias),   sourceFolder );
    settings.setValue(QString("%1/targetPath").arg(alias),  targetPath );
    settings.setValue(QString("%1/backend").arg(alias),     backend );
    settings.setValue(QString("%1/connection").arg(alias),  QString::fromLocal8Bit("ownCloud"));
    settings.setValue(QString("%1/onlyThisLAN").arg(alias), onlyThisLAN );
    settings.sync();

}

void FolderMan::slotRemoveFolder( const QString& alias )
{
    if( alias.isEmpty() ) return;

    if( _currentSyncFolder == alias ) {
        // attention: sync is currently running!
        _folderToDelete = true; // flag for the sync finished slot
    } else {
        removeFolder(alias);
    }
}

// remove a folder from the map. Should be sure n
void FolderMan::removeFolder( const QString& alias )
{
    if( _folderMap.contains( alias )) {
      qDebug() << "Removing " << alias;
      Folder *f = _folderMap.take( alias );
      f->deleteLater();
    } else {
      qDebug() << "!! Can not remove " << alias << ", not in folderMap.";
    }

    QFile file( _folderConfigPath + "/" + alias );
    if( file.exists() ) {
        qDebug() << "Remove folder config file " << file.fileName();
      file.remove();
    }
}

}
