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
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QUrl>
#include <QSignalMapper>

#include "mirall/mirallconfigfile.h"
#include "mirall/unisonfolder.h"
#include "mirall/csyncfolder.h"
#include "mirall/owncloudfolder.h"
#include "mirall/syncresult.h"
#include "mirall/folderman.h"

namespace Mirall {

FolderMan::FolderMan(QObject *parent) :
    QObject(parent)
{
    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    storageDir.mkpath("folders");
    _folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

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
            QUrl url( cfgFile.fullOwnCloudUrl() );
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
    connect(folder, SIGNAL(syncStateChange()), _folderChangeSignalMapper, SLOT(map()));
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
      f->setSyncEnabled( _folderEnabledMap.value( f->alias() ));
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

void FolderMan::slotFolderSyncStarted( )
{
}

void FolderMan::slotFolderSyncFinished( const SyncResult& )
{

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
Folder* FolderMan::addFolderDefinition( const QString& backend, const QString& alias,
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

    Folder *folder = setupFolderFromConfigFile(alias);
    //    setupContextMenu(); FIXME: Refresh GUI elements.

    return folder;
}

void FolderMan::slotRemoveFolder( const QString& alias )
{
    if( alias.isEmpty() ) return;

    if( _folderMap.contains( alias )) {
      qDebug() << "Removing " << alias;
      Folder *f = _folderMap.take( alias );
      delete f;
    } else {
      qDebug() << "!! Can not remove " << alias << ", not in folderMap.";
    }

    QFile file( _folderConfigPath + "/" + alias );
    if( file.exists() ) {
        qDebug() << "Remove folder config file " << file.fileName();
      file.remove();
    }
    // FIXME: Refresh GUI elements

}

}
#include "folderman.moc"

