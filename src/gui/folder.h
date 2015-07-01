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
#include "syncfilestatus.h"
#include "networkjobs.h"

#include <csync.h>

#include <QDir>
#include <QHash>
#include <QSet>
#include <QObject>
#include <QStringList>

#include <QDebug>
#include <QTimer>
#include <qelapsedtimer.h>

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
    {}

    /// The name of the folder in the ui and internally
    QString alias;
    /// path on local machine
    QString localPath;
    /// path on remote
    QString targetPath;
    /// whether the folder is paused
    bool paused;

    /// Saves the folder definition, creating a new settings group.
    static void save(QSettings& settings, const FolderDefinition& folder);

    /// Reads a folder definition from a settings group with the name 'alias'.
    static bool load(QSettings& settings, const QString& alias,
                     FolderDefinition* folder);
};

/**
 * @brief The Folder class
 * @ingroup gui
 */
class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const FolderDefinition& definition, QObject* parent = 0L);

    ~Folder();

    typedef QMap<QString, Folder*> Map;
    typedef QMapIterator<QString, Folder*> MapIterator;

    /**
     * The account the folder is configured on.
     */
    void setAccountState( AccountState *account );
    AccountState* accountState() const;

    /**
     * alias or nickname
     */
    QString alias() const;

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
     * local folder path with native separators
     */
    QString nativePath() const;

    /**
     * switch sync on or off
     * If the sync is switched off, the startSync method is not going to
     * be called.
     */
    void setSyncPaused( bool );

    bool syncPaused() const;

    /**
     * Returns true when the folder may sync.
     *
     * !syncPaused() && accountState->isConnected().
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

     // Used by the Socket API
     SyncJournalDb *journalDb() { return &_journal; }

     bool estimateState(QString fn, csync_ftw_type_e t, SyncFileStatus* s);

     RequestEtagJob *etagJob() { return _requestEtagJob; }
     qint64 msecSinceLastSync() const { return _timeSinceLastSyncDone.elapsed(); }
     qint64 msecLastSyncDuration() const { return _lastSyncDuration; }
     int consecutiveFollowUpSyncs() const { return _consecutiveFollowUpSyncs; }

     /// Saves the folder data in the account's settings.
     void saveToSettings() const;
     /// Removes the folder from the account's settings.
     void removeFromSettings() const;

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync(Folder*);
    void progressInfo(const ProgressInfo& progress);
    void newSharedBigFolderDiscovered(const QString &); // A new folder bigger than the threshold was discovered

public slots:

     /**
       * terminate the current sync run
       */
     void slotTerminateSync();

     void slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool*);


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
    void slotSyncFinished();

    void slotFolderDiscovered(bool local, QString folderName);
    void slotTransmissionProgress(const ProgressInfo& pi);
    void slotJobCompleted(const SyncFileItem&);
    void slotSyncItemDiscovered(const SyncFileItem & item);

    void slotRunEtagJob();
    void etagRetreived(const QString &);
    void etagRetreivedFromSyncEngine(const QString &);

    void slotAboutToPropagate(SyncFileItemVector& );
    void slotThreadTreeWalkResult(const SyncFileItemVector& ); // after sync is done

    void slotEmitFinishedDelayed();

    void watcherSlot(QString);
    void slotNewSharedBigFolderDiscovered(const QString &);

private:
    bool init();

    bool setIgnoredFiles();

    void bubbleUpSyncResult();

    void checkLocalPath();

    void createGuiLog(const QString& filename, SyncFileStatus status, int count,
                       const QString& renameTarget = QString::null );

    AccountState* _accountState;
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

    // For the SocketAPI folder states
    QSet<QString>   _stateLastSyncItemsWithErrorNew; // gets moved to _stateLastSyncItemsWithError at end of sync
    QSet<QString>   _stateLastSyncItemsWithError;
    QSet<QString>   _stateTaintedFolders;

    SyncJournalDb _journal;

    ClientProxy   _clientProxy;

    CSYNC *_csync_ctx;
};

}

#endif
