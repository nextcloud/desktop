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

#include "folder.h"
#include "folderwatcher.h"
#include "syncfileitem.h"

class QSignalMapper;
class TestFolderMan;

namespace OCC {

class Application;
class SyncResult;
class SocketApi;
class LockWatcher;

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

    /**
      * returns a list of local files that exist on the local harddisk for an
      * incoming relative server path. The method checks with all existing sync
      * folders.
      */
    QStringList findFileInLocalFolders( const QString& relPath, const AccountPtr acc );

    /** Returns the folder by alias or NULL if no folder with the alias exists. */
    Folder *folder( const QString& );

    /**
     * Migrate accounts from owncloud < 2.0
     * Creates a folder for a specific configuration, identified by alias.
     */
    Folder* setupFolderFromOldConfigFile(const QString &, AccountState *account );

    /**
     * Ensures that a given directory does not contain a sync journal file.
     *
     * @returns false if the journal could not be removed, true otherwise.
     */
    static bool ensureJournalGone(const QString& journalDbFile);

    /** Creates a new and empty local directory. */
    bool startFromScratch( const QString& );

    QString statusToString(SyncResult, bool paused ) const;

    static SyncResult accountStatus( const QList<Folder*> &folders );

    void removeMonitorPath( const QString& alias, const QString& path );
    void addMonitorPath( const QString& alias, const QString& path );

    // Escaping of the alias which is used in QSettings AND the file
    // system, thus need to be escaped.
    static QString escapeAlias( const QString& );
    static QString unescapeAlias( const QString& );

    SocketApi *socketApi();

    /**
     * Check if @a path is a valid path for a new folder considering the already sync'ed items.
     * Make sure that this folder, or any subfolder is not sync'ed already.
     *
     * \a forNewDirectory is internal and is used for recursion.
     *
     * @returns an empty string if it is allowed, or an error if it is not allowed
     */
    QString checkPathValidityForNewFolder(const QString &path, const QUrl& serverUrl = QUrl(), bool forNewDirectory = false);

    /**
     * While ignoring hidden files can theoretically be switched per folder,
     * it's currently a global setting that users can only change for all folders
     * at once.
     * These helper functions can be removed once it's properly per-folder.
     */
    bool ignoreHiddenFiles() const;
    void setIgnoreHiddenFiles(bool ignore);

    /**
     * Access to the current queue of scheduled folders.
     */
    QQueue<Folder*> scheduleQueue() const;

    /**
     * Access to the currently syncing folder.
     */
    Folder* currentSyncFolder() const;

signals:
    /**
      * signal to indicate a folder has changed its sync state.
      *
      * Attention: The folder may be zero. Do a general update of the state then.
      */
    void folderSyncStateChange(Folder*);

    /**
     * Indicates when the schedule queue changes.
     */
    void scheduleQueueChanged();

    void folderListChanged(const Folder::Map &);

public slots:
    void slotRemoveFolder( Folder* );
    void slotFolderSyncPaused(Folder *, bool paused);
    void slotFolderCanSyncChanged();

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
    // slot to schedule an ETag job
    void slotScheduleETagJob ( const QString &alias, RequestEtagJob *job);
    void slotEtagJobDestroyed (QObject*);
    void slotRunOneEtagJob();

    /**
     * Schedules folders of newly connected accounts, terminates and
     * de-schedules folders of disconnected accounts.
     */
    void slotAccountStateChanged();

    /**
     * restart the client as soon as it is possible, ie. no folders syncing.
     */
    void slotScheduleAppRestart();

    /**
     * Triggers a sync run once the lock on the given file is removed.
     *
     * Automatically detemines the folder that's responsible for the file.
     */
    void slotSyncOnceFileUnlocks(const QString& path);

private slots:
    // slot to take the next folder from queue and start syncing.
    void slotStartScheduledFolderSync();
    void slotEtagPollTimerTimeout();

    void slotRemoveFoldersForAccount(AccountState* accountState);

    // Wraps the Folder::syncStateChange() signal into the
    // FolderMan::folderSyncStateChange(Folder*) signal.
    void slotForwardFolderSyncStateChange();

    void slotServerVersionChanged(Account* account);

    /**
     * Schedules the folder for synchronization that contains
     * the file with the given path.
     */
    void slotScheduleFolderOwningFile(const QString& path);

private:
    /** Adds a new folder, does not add it to the account settings and
     *  does not set an account on the new folder.
      */
    Folder* addFolderInternal(FolderDefinition folderDefinition, AccountState* accountState);

    /* unloads a folder object, does not delete it */
    void unloadFolder( Folder * );

    /** Will start a sync after a bit of delay. */
    void startScheduledSyncSoon(qint64 msMinimumDelay = 0);

    // finds all folder configuration files
    // and create the folders
    QString getBackupName( QString fullPathName ) const;
    void registerFolderMonitor( Folder *folder );

    // restarts the application (Linux only)
    void restartApplication();

    QSet<Folder*>  _disabledFolders;
    Folder::Map    _folderMap;
    QString        _folderConfigPath;
    Folder        *_currentSyncFolder;
    QPointer<Folder> _lastSyncFolder;
    bool           _syncEnabled;
    QTimer         _etagPollTimer;
    QPointer<RequestEtagJob>        _currentEtagJob; // alias of Folder running the current RequestEtagJob

    QMap<QString, FolderWatcher*> _folderWatchers;
    QScopedPointer<LockWatcher> _lockWatcher;
    QScopedPointer<SocketApi> _socketApi;

    /** The aliases of folders that shall be synced. */
    QQueue<Folder*> _scheduleQueue;

    /** When the timer expires one of the scheduled syncs will be started. */
    QTimer          _startScheduledSyncTimer;

    bool            _appRestartRequired;

    static FolderMan *_instance;
    explicit FolderMan(QObject *parent = 0);
    friend class OCC::Application;
    friend class ::TestFolderMan;
};

} // namespace OCC
#endif // FOLDERMAN_H
