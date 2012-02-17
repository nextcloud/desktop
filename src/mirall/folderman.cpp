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

#include "mirall/unisonfolder.h"
#include "mirall/csyncfolder.h"
#include "mirall/owncloudfolder.h"
#include "mirall/syncresult.h"
#include "mirall/folderman.h"

namespace Mirall {

FolderMan::FolderMan(QObject *parent) :
    QObject(parent),
    _folderSyncCount(0)

{
    _folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    storageDir.mkpath("folders");
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
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));

    // setup a handler to look for configuration changes
    _configFolderWatcher = new FolderWatcher(storageDir.path());
    connect(_configFolderWatcher, SIGNAL(folderChanged(const QStringList &)),
            this, SLOT(slotReparseConfiguration()));

    return setupKnownFolders();

}

QString FolderMan::folderConfigPath() const
{
    return _folderConfigPath;
}


int FolderMan::setupKnownFolders()
{
  qDebug() << "* Setup folders from " << folderConfigPath();

  _folderMap.clear();
  QDir dir(folderConfigPath());
  dir.setFilter(QDir::Files);
  QStringList list = dir.entryList();
  foreach (QString file, list) {
    setupFolderFromConfigFile(file);
  }
}

// filename is the name of the file only, it does not include
// the configuration directory path
Folder* FolderMan::setupFolderFromConfigFile(const QString &file) {
    Folder *folder = 0L;

    qDebug() << "  ` -> setting up:" << file;
    QSettings settings(folderConfigPath() + "/" + file, QSettings::IniFormat);
    qDebug() << "    -> file path: " + settings.fileName();

    if (!settings.contains("folder/path")) {
        qWarning() << "   `->" << file << "is not a valid folder configuration";
        return folder;
    }

    QVariant path = settings.value("folder/path").toString();
    if (path.isNull() || !QFileInfo(path.toString()).isDir()) {
        qWarning() << "    `->" << path.toString() << "does not exist. Skipping folder" << file;
        // _tray->showMessage(tr("Unknown folder"),
        //                    tr("Folder %1 does not exist").arg(path.toString()),
        //                    QSystemTrayIcon::Critical);
        return folder;
    }

    QString backend = settings.value("folder/backend").toString();
    if (!backend.isEmpty()) {
        if( backend == "sitecopy") {
            qCritical() << "* sitecopy is not longer supported in this release." << endl;
        } else if (backend == "unison") {
            folder = new UnisonFolder(file,
                                      path.toString(),
                                      settings.value("backend:unison/secondPath").toString(),
                                      this);
        } else if (backend == "csync") {
#ifdef WITH_CSYNC
            folder = new CSyncFolder(file,
                                     path.toString(),
                                     settings.value("backend:csync/secondPath").toString(),
                                     this);
#else
            qCritical() << "* csync support not enabled!! ignoring:" << file;
#endif
        } else if( backend == "owncloud" ) {
#ifdef WITH_CSYNC
            QUrl url; // ( _owncloudSetup->fullOwnCloudUrl() );
            QString existPath = url.path();
            qDebug() << "existing path: "  << existPath;
            QString newPath = settings.value("backend:owncloud/targetPath").toString();
            if( !existPath.isEmpty() ) {
                // cut off the trailing slash
                if( existPath.endsWith('/') ) {
                    existPath.truncate( existPath.length()-1 );
                }
                // cut off the leading slash
                if( newPath.startsWith('/') ) {
                    newPath.remove(0,1);
                }
            }

            url.setPath( QString("%1/files/webdav.php/%2").arg(existPath).arg(newPath) );

            folder = new ownCloudFolder( file, path.toString(),
                                         url.toString(),
                                         this );


#else
            qCritical() << "* owncloud support not enabled!! ignoring:" << file;
#endif
        }

        else {
            qWarning() << "unknown backend" << backend;
            return NULL;
        }
    }
    folder->setBackend( backend );
    folder->setOnlyOnlineEnabled(settings.value("folder/onlyOnline", false).toBool());
    folder->setOnlyThisLANEnabled(settings.value("folder/onlyThisLAN", false).toBool());

    _folderMap[file] = folder;
    qDebug() << "Adding folder to Folder Map " << folder;
    QObject::connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    QObject::connect(folder, SIGNAL(syncFinished(const SyncResult &)), SLOT(slotFolderSyncFinished(const SyncResult &)));

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

SyncResult FolderMan::syncResult( const QString& alias )
{
    SyncResult res;
    if( _folderMap.contains( alias ) ) {

    }
    return res;
}

void FolderMan::slotFolderSyncStarted( )
{
     _folderSyncCount++;
}

void FolderMan::slotFolderSyncFinished( const SyncResult& )
{
    _folderSyncCount--;
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
    QSettings settings(folderConfigPath() + "/" + alias, QSettings::IniFormat);

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

    QFile file( folderConfigPath() + "/" + alias );
    if( file.exists() ) {
        qDebug() << "Remove folder config file " << file.fileName();
      file.remove();
    }
    // FIXME: Refresh GUI elements

}

}
#include "folderman.moc"

