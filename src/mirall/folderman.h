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
#include <QList>

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/syncfileitem.h"

class QSignalMapper;

class SyncResult;

namespace Mirall {

class Application;

class OWNCLOUDSYNC_EXPORT FolderMan : public QObject
{
    Q_OBJECT
public:
    static FolderMan* instance();

    int setupFolders();

    Mirall::Folder::Map map();

    /**
      * Add a folder definition to the config
      * Params:
      * QString alias
      * QString sourceFolder on local machine
      * QString targetPath on remote
      */
    void addFolderDefinition(const QString&, const QString&, const QString& );

    /** Returns the folder which the file or directory stored in path is in */
    Folder* folderForPath(const QUrl& path);

    /** Returns the folder by alias or NULL if no folder with the alias exists. */
    Folder *folder( const QString& );

    /** Returns the last sync result by alias */
    SyncResult syncResult( const QString& );

    /** Creates a folder for a specific configuration, identified by alias. */
    Folder* setupFolderFromConfigFile(const QString & );

    /** Wipes all folder defintions. No way back! */
    void removeAllFolderDefinitions();

    /**
     * Ensures that a given directory does not contain a .csync_journal.
     *
     * @returns false if the journal could not be removed, true otherwise.
     */
    static bool ensureJournalGone(const QString &path);

    /** Creates a new and empty local directory. */
    bool startFromScratch( const QString& );

    QString statusToString( SyncResult, bool enabled ) const;

    static SyncResult accountStatus( const QList<Folder*> &folders );

    void removeMonitorPath( const QString& alias, const QString& path );
    void addMonitorPath( const QString& alias, const QString& path );

signals:
    /**
      * signal to indicate a folder named by alias has changed its sync state.
      * Get the state via the Folder Map or the syncResult and syncState methods.
      */
    void folderSyncStateChange( const QString & );

    void folderListLoaded(const Folder::Map &);

public slots:
    void slotRemoveFolder( const QString& );
    void slotEnableFolder( const QString&, bool );

    void slotFolderSyncStarted();
    void slotFolderSyncFinished( const SyncResult& );

    void terminateSyncProcess( const QString& alias = QString::null );

    /* delete all folder objects */
    int unloadAllFolders();

    // if enabled is set to false, no new folders will start to sync.
    // the current one will finish.
    void setSyncEnabled( bool );

    void slotScheduleAllFolders();

    void setDirtyProxy(bool value = true);
    void setDirtyNetworkLimits();

    // slot to add a folder to the syncing queue
    void slotScheduleSync( const QString & );

private slots:

    // slot to take the next folder from queue and start syncing.
    void slotScheduleFolderSync();

private:
    // finds all folder configuration files
    // and create the folders
    void terminateCurrentSync();
    QString getBackupName( const QString& ) const;
    void registerFolderMonitor( Folder *folder );

    // Escaping of the alias which is used in QSettings AND the file
    // system, thus need to be escaped.
    QString escapeAlias( const QString& ) const;
    QString unescapeAlias( const QString& ) const;

    void removeFolder( const QString& );

    QSet<Folder*>  _disabledFolders;
    Folder::Map    _folderMap;
    QString        _folderConfigPath;
    QSignalMapper *_folderChangeSignalMapper;
    QSignalMapper *_folderWatcherSignalMapper;
    QString        _currentSyncFolder;
    bool           _syncEnabled;
    QQueue<QString> _scheduleQueue;
    QMap<QString, FolderWatcher*> _folderWatchers;

    static FolderMan *_instance;
    explicit FolderMan(QObject *parent = 0);
    ~FolderMan();
    friend class Mirall::Application;
};

} // namespace Mirall
#endif // FOLDERMAN_H
