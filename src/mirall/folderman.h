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
#include <QQueue>

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/syncfileitem.h"

class QSignalMapper;

namespace Mirall {

class SyncResult;

class FolderMan : public QObject
{
    Q_OBJECT
public:
    explicit FolderMan(QObject *parent = 0);
    ~FolderMan();

    int setupFolders();

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
    void addFolderDefinition( const QString&, const QString&, const QString&, const QString&, bool );

    /**
      * return the folder by alias or NULL if no folder with the alias exists.
      */
    Folder *folder( const QString& );

    /**
      * return the last sync result by alias
      */
    SyncResult syncResult( const QString& );

    /**
      * creates a folder for a specific configuration, identified by alias.
      */
    Folder* setupFolderFromConfigFile(const QString & );

    /**
     * wipes all folder defintions. No way back!
     */
    void removeAllFolderDefinitions();

    /**
     * Removes csync journals from all folders.
     */
    void wipeAllJournals();

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

    void terminateSyncProcess( const QString& );

    // if enabled is set to false, no new folders will start to sync.
    // the current one will finish.
    void setSyncEnabled( bool );

    void slotScheduleAllFolders();

private slots:
    // slot to add a folder to the syncing queue
    void slotScheduleSync( const QString & );

    // slot to take the next folder from queue and start syncing.
    void slotScheduleFolderSync();

private:
    // finds all folder configuration files
    // and create the folders
    int setupKnownFolders();
    void terminateCurrentSync();

    // Escaping of the alias which is used in QSettings AND the file
    // system, thus need to be escaped.
    QString escapeAlias( const QString& ) const;
    QString unescapeAlias( const QString& ) const;

    void removeFolder( const QString& );

    FolderWatcher *_configFolderWatcher;
    Folder::Map    _folderMap;
    QString        _folderConfigPath;
    QSignalMapper *_folderChangeSignalMapper;
    QString        _currentSyncFolder;
    QStringList    _scheduleQueue;
    bool           _syncEnabled;
};

}
#endif // FOLDERMAN_H
