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

/**
 * @brief The FolderMan class
 * @ingroup gui
 */
class FolderMan : public QObject
{
    Q_OBJECT
public:
    ~FolderMan();
    static FolderMan* instance();

    int setupFolders();
    int setupFoldersMigration();

    OCC::Folder::Map map();

    /** Adds a folder for an account, ensures the journal is gone and saves it in the settings.
      */
    Folder* addFolder(AccountState* accountState, const FolderDefinition& folderDefinition);

    /** Returns the folder which the file or directory stored in path is in */
    Folder* folderForPath(const QString& path);

    /** Returns the folder by alias or NULL if no folder with the alias exists. */
    Folder *folder( const QString& );

    /**
     * Migrate accounts from owncloud < 2.0
     * Creates a folder for a specific configuration, identified by alias.
     */
    Folder* setupFolderFromOldConfigFile(const QString &, AccountState *account );

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
      * signal to indicate a folder has changed its sync state.
      *
      * Attention: The folder may be zero. Do a general update of the state than.
      */
    void folderSyncStateChange(Folder*);

    void folderListLoaded(const Folder::Map &);

public slots:
    void slotRemoveFolder( Folder* );
    void slotSetFolderPaused(Folder *, bool paused);

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
    void slotScheduleSync(Folder*);
    // slot to scheule an ETag job
    void slotScheduleETagJob ( const QString &alias, RequestEtagJob *job);
    void slotEtagJobDestroyed (QObject*);
    void slotRunOneEtagJob();

    /**
     * Schedules folders of newly connected accounts, terminates and
     * de-schedules folders of disconnected accounts.
     */
    void slotAccountStateChanged();

private slots:

    // slot to take the next folder from queue and start syncing.
    void slotStartScheduledFolderSync();
    void slotEtagPollTimerTimeout();

    void slotRemoveFoldersForAccount(AccountState* accountState);

    // Wraps the Folder::syncStateChange() signal into the
    // FolderMan::folderSyncStateChange(Folder*) signal.
    void slotForwardFolderSyncStateChange();

private:
    /** Adds a new folder, does not add it to the account settings and
     *  does not set an account on the new folder.
      */
    Folder* addFolderInternal(const FolderDefinition& folderDefinition);

    /* unloads a folder object, does not delete it */
    void unloadFolder( Folder * );

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
    Folder        *_currentSyncFolder;
    QPointer<Folder> _lastSyncFolder;
    bool           _syncEnabled;
    QTimer         _etagPollTimer;
    QPointer<RequestEtagJob>        _currentEtagJob; // alias of Folder running the current RequestEtagJob

    QMap<QString, FolderWatcher*> _folderWatchers;
    QPointer<SocketApi> _socketApi;

    /** The aliases of folders that shall be synced. */
    QQueue<Folder*> _scheduleQueue;

    /** When the timer expires one of the scheduled syncs will be started. */
    QTimer          _startScheduledSyncTimer;

    static FolderMan *_instance;
    explicit FolderMan(QObject *parent = 0);
    friend class OCC::Application;
};

} // namespace OCC
#endif // FOLDERMAN_H
