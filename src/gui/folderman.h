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
#include <QPointer>

#include "folder.h"
#include "folderwatcher.h"
#include "syncfileitem.h"

class QSignalMapper;

namespace OCC {

class Application;
class SyncResult;
class SocketApi;

class FolderMan : public QObject
{
    Q_OBJECT
public:
    ~FolderMan();
    static FolderMan* instance();

    int setupFolders();

    OCC::Folder::Map map();

    /**
      * Add a folder definition to the config
      * Params:
      * QString alias
      * QString sourceFolder on local machine
      * QString targetPath on remote
      *
      * Ensures any existing journal in the sourceFolder is deleted.
      * Returns true on success.
      */
    bool addFolderDefinition(const QString& alias, const QString& sourceFolder,
                             const QString& targetPath,
                             const QStringList& selectiveSyncBlacklist = QStringList());

    /** Returns the folder which the file or directory stored in path is in */
    Folder* folderForPath(const QString& path);

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

    QString statusToString(SyncResult, bool paused ) const;

    static SyncResult accountStatus( const QList<Folder*> &folders );

    void removeMonitorPath( const QString& alias, const QString& path );
    void addMonitorPath( const QString& alias, const QString& path );

    // Escaping of the alias which is used in QSettings AND the file
    // system, thus need to be escaped.
    static QString escapeAlias( const QString& );

    SocketApi *socketApi();

signals:
    /**
      * signal to indicate a folder named by alias has changed its sync state.
      * Get the state via the Folder Map or the syncResult and syncState methods.
      *
      * Attention: The alias string may be zero. Do a general update of the state than.
      */
    void folderSyncStateChange( const QString & );

    void folderListLoaded(const Folder::Map &);

public slots:
    void slotRemoveFolder( const QString& );
    void slotSetFolderPaused(const QString&, bool paused);

    void slotFolderSyncStarted();
    void slotFolderSyncFinished( const SyncResult& );

    /**
     * Terminates the current folder sync.
     *
     * It does not switch the folder to paused state.
     */
    void terminateSyncProcess();

    /* delete all folder objects */
    int unloadAndDeleteAllFolders();

    // if enabled is set to false, no new folders will start to sync.
    // the current one will finish.
    void setSyncEnabled( bool );

    void slotScheduleAllFolders();

    void setDirtyProxy(bool value = true);
    void setDirtyNetworkLimits();

    // slot to add a folder to the syncing queue
    void slotScheduleSync( const QString & );
    // slot to scheule an ETag job
    void slotScheduleETagJob ( const QString &alias, RequestEtagJob *job);
    void slotEtagJobDestroyed (QObject*);
    void slotRunOneEtagJob();

private slots:

    // slot to take the next folder from queue and start syncing.
    void slotStartScheduledFolderSync();
    void slotEtagPollTimerTimeout();
    void slotRemoveFoldersForAccount(AccountState* accountState);

private:
    /* unloads a folder object, does not delete it */
    void unloadFolder( const QString& alias );

    /** Will start a sync after a bit of delay. */
    void startScheduledSyncSoon(qint64 msMinimumDelay = 0);

    // finds all folder configuration files
    // and create the folders
    QString getBackupName( QString fullPathName ) const;
    void registerFolderMonitor( Folder *folder );

    QString unescapeAlias( const QString& ) const;

    QSet<Folder*>  _disabledFolders;
    Folder::Map    _folderMap;
    QString        _folderConfigPath;
    QSignalMapper *_folderChangeSignalMapper;
    QString        _currentSyncFolder;
    QString        _lastSyncFolder;
    bool           _syncEnabled;
    QTimer         _etagPollTimer;
    QPointer<RequestEtagJob>        _currentEtagJob; // alias of Folder running the current RequestEtagJob

    QMap<QString, FolderWatcher*> _folderWatchers;
    QPointer<SocketApi> _socketApi;

    /** The aliases of folders that shall be synced. */
    QQueue<QString> _scheduleQueue;

    /** When the timer expires one of the scheduled syncs will be started. */
    QTimer          _startScheduledSyncTimer;

    static FolderMan *_instance;
    explicit FolderMan(QObject *parent = 0);
    friend class OCC::Application;
};

} // namespace OCC
#endif // FOLDERMAN_H
