/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_FOLDER_H
#define MIRALL_FOLDER_H

#include "syncresult.h"
#include "progressdispatcher.h"
#include "syncjournaldb.h"
#include "clientproxy.h"
#include "networkjobs.h"

#include <csync.h>

#include <QObject>
#include <QStringList>

class QThread;
class QSettings;

namespace OCC {

class SyncEngine;
class AccountState;

/**
 * @brief The FolderDefinition class
 * @ingroup gui
 */
class FolderDefinition
{
public:
    FolderDefinition()
        : paused(false)
        , ignoreHiddenFiles(true)
    {}

    /// The name of the folder in the ui and internally
    QString alias;
    /// path on local machine
    QString localPath;
    /// path on remote
    QString targetPath;
    /// whether the folder is paused
    bool paused;
    /// whether the folder syncs hidden files
    bool ignoreHiddenFiles;

    /// Saves the folder definition, creating a new settings group.
    static void save(QSettings& settings, const FolderDefinition& folder);

    /// Reads a folder definition from a settings group with the name 'alias'.
    static bool load(QSettings& settings, const QString& alias,
                     FolderDefinition* folder);

    /// Ensure / as separator and trailing /.
    static QString prepareLocalPath(const QString& path);
};

/**
 * @brief The Folder class
 * @ingroup gui
 */
class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const FolderDefinition& definition, AccountState* accountState, QObject* parent = 0L);

    ~Folder();

    typedef QMap<QString, Folder*> Map;
    typedef QMapIterator<QString, Folder*> MapIterator;

    /**
     * The account the folder is configured on.
     */
    AccountState* accountState() const { return _accountState.data(); }

    /**
     * alias or nickname
     */
    QString alias() const;
    QString shortGuiRemotePathOrAppName() const; // since 2.0 we don't want to show aliases anymore, show the path instead

    /**
     * short local path to display on the GUI  (native separators)
     */
    QString shortGuiLocalPath() const;

    /**
     * local folder path
     */
    QString path() const;

    /**
     * wrapper for QDir::cleanPath("Z:\\"), which returns "Z:\\", but we need "Z:" instead
     */
    QString cleanPath();

    /**
     * remote folder path
     */
    QString remotePath() const;

    /**
     * remote folder path with server url
     */
    QUrl remoteUrl() const;

    /**
     * switch sync on or off
     */
    void setSyncPaused( bool );

    bool syncPaused() const;

    /**
     * Returns true when the folder may sync.
     */
    bool canSync() const;

    void prepareToSync();

    /**
     * True if the folder is busy and can't initiate
     * a synchronization
     */
    virtual bool isBusy() const;

    /**
     * return the last sync result with error message and status
     */
     SyncResult syncResult() const;

     /**
      * set the config file name.
      */
     void setConfigFile( const QString& );
     QString configFile();

     /**
      * This is called if the sync folder definition is removed. Do cleanups here.
      */
     virtual void wipe();

     void setSyncState(SyncResult::Status state);

     void setDirtyNetworkLimits();

     /**
      * Ignore syncing of hidden files or not. This is defined in the
      * folder definition
      */
     bool ignoreHiddenFiles();
     void setIgnoreHiddenFiles(bool ignore);

     // Used by the Socket API
     SyncJournalDb *journalDb() { return &_journal; }
     SyncEngine &syncEngine() { return *_engine; }

     RequestEtagJob *etagJob() { return _requestEtagJob; }
     qint64 msecSinceLastSync() const { return _timeSinceLastSyncDone.elapsed(); }
     qint64 msecLastSyncDuration() const { return _lastSyncDuration; }
     int consecutiveFollowUpSyncs() const { return _consecutiveFollowUpSyncs; }

     /// Saves the folder data in the account's settings.
     void saveToSettings() const;
     /// Removes the folder from the account's settings.
     void removeFromSettings() const;

     /**
      * Returns whether a file inside this folder should be excluded.
      */
     bool isFileExcludedAbsolute(const QString& fullPath) const;

     /**
      * Returns whether a file inside this folder should be excluded.
      */
     bool isFileExcludedRelative(const QString& relativePath) const;

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync(Folder*);
    void progressInfo(const ProgressInfo& progress);
    void newBigFolderDiscovered(const QString &); // A new folder bigger than the threshold was discovered
    void syncPausedChanged(Folder*, bool paused);
    void canSyncChanged();

    /**
     * Fires for each change inside this folder that wasn't caused
     * by sync activity.
     */
    void watchedFileChangedExternally(const QString& path);

public slots:

     /**
       * terminate the current sync run
       */
     void slotTerminateSync();

     // connected to the corresponding signals in the SyncEngine
     void slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool*);
     void slotAboutToRestoreBackup(bool*);


     /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
      void startSync(const QStringList &pathList = QStringList());

      void setProxyDirty(bool value);
      bool proxyDirty();

      int slotDiscardDownloadProgress();
      int downloadInfoCount();
      int slotWipeErrorBlacklist();
      int errorBlackListEntryCount();

      /**
       * Triggered by the folder watcher when a file/dir in this folder
       * changes. Needs to check whether this change should trigger a new
       * sync run to be scheduled.
       */
      void slotWatchedPathChanged(const QString& path);

private slots:
    void slotSyncStarted();
    void slotSyncError(const QString& );
    void slotCsyncUnavailable();
    void slotSyncFinished(bool);

    void slotFolderDiscovered(bool local, QString folderName);
    void slotTransmissionProgress(const ProgressInfo& pi);
    void slotItemCompleted(const SyncFileItem&, const PropagatorJob&);

    void slotRunEtagJob();
    void etagRetreived(const QString &);
    void etagRetreivedFromSyncEngine(const QString &);

    void slotThreadTreeWalkResult(const SyncFileItemVector& ); // after sync is done

    void slotEmitFinishedDelayed();

    void slotNewBigFolderDiscovered(const QString &);

private:
    bool setIgnoredFiles();

    void bubbleUpSyncResult();

    void checkLocalPath();

    enum LogStatus {
        LogStatusRemove,
        LogStatusRename,
        LogStatusMove,
        LogStatusNew,
        LogStatusError,
        LogStatusConflict,
        LogStatusUpdated
    };

    void createGuiLog(const QString& filename, LogStatus status, int count,
                       const QString& renameTarget = QString::null );

    AccountStatePtr _accountState;
    FolderDefinition _definition;

    SyncResult _syncResult;
    QScopedPointer<SyncEngine> _engine;
    QStringList  _errors;
    bool         _csyncError;
    bool         _csyncUnavail;
    bool         _wipeDb;
    bool         _proxyDirty;
    QPointer<RequestEtagJob> _requestEtagJob;
    QString       _lastEtag;
    QElapsedTimer _timeSinceLastSyncDone;
    QElapsedTimer _timeSinceLastSyncStart;
    qint64        _lastSyncDuration;
    bool          _forceSyncOnPollTimeout;

    /// The number of syncs that failed in a row.
    /// Reset when a sync is successful.
    int           _consecutiveFailingSyncs;

    /// The number of requested follow-up syncs.
    /// Reset when no follow-up is requested.
    int           _consecutiveFollowUpSyncs;

    SyncJournalDb _journal;

    ClientProxy   _clientProxy;
};

}

#endif
