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


#ifndef FOLDERMAN_H
#define FOLDERMAN_H

#include <QObject>

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"

class QSignalMapper;

namespace Mirall {

class SyncResult;
class OwncloudSetup;

class FolderMan : public QObject
{
    Q_OBJECT
public:
    explicit FolderMan(QObject *parent = 0);
    ~FolderMan();

    int setupFolders();
    void disableFoldersWithRestore();
    void restoreEnabledFolders();

    Mirall::Folder::Map map();

    /**
      * Add a folder definition to the config
      * Params:
      * QString backend
      * QString alias
      * QString sourceFolder on local machine
      * QString targetPath on remote
      * bool    onlyThisLAN, currently unused.
      */
    Folder *addFolderDefinition( const QString&, const QString&, const QString&, const QString&, bool );

    /**
      * return the last sync result by alias
      */
    SyncResult syncResult( const QString& );

signals:
    /**
      * signal to indicate a folder named by alias has changed its sync state.
      * Get the state via the Folder Map or the syncResult and syncState methods.
      */
    void folderSyncStateChange( const QString & );

public slots:
    void slotRemoveFolder( const QString& );
    void slotEnableFolder( const QString&, bool );

    void slotFolderSyncStarted();
    void slotFolderSyncFinished( const SyncResult& );

    void slotReparseConfiguration();

private:
    // finds all folder configuration files
    // and create the folders
    int setupKnownFolders();

    // creates a folder for a specific
    // configuration
    Folder* setupFolderFromConfigFile(const QString & );

    FolderWatcher *_configFolderWatcher;
    Folder::Map _folderMap;
    QHash<QString, bool> _folderEnabledMap;
    QString _folderConfigPath;
    OwncloudSetup *_ownCloudSetup;
    QSignalMapper *_folderChangeSignalMapper;

};

}
#endif // FOLDERMAN_H
