/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef CSYNCTHREAD_H
#define CSYNCTHREAD_H

#include <stdint.h>

#include <QMutex>
#include <QThread>
#include <QString>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QSharedPointer>

#include <csync.h>

// when do we go away with this private/public separation?
#include <csync_private.h>

#include "excludedfiles.h"
#include "syncfileitem.h"
#include "progressdispatcher.h"
#include "utility.h"
#include "syncfilestatustracker.h"
#include "accountfwd.h"
#include "discoveryphase.h"
#include "checksums.h"

class QProcess;

namespace OCC {

class SyncJournalFileRecord;
class SyncJournalDb;
class OwncloudPropagator;
class PropagatorJob;

/**
 * @brief The SyncEngine class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncEngine : public QObject
{
    Q_OBJECT
public:
    SyncEngine(AccountPtr account, const QString &localPath,
               const QString &remotePath, SyncJournalDb *journal);
    ~SyncEngine();

    static QString csyncErrorToString( CSYNC_STATUS);

    Q_INVOKABLE void startSync();
    void setNetworkLimits(int upload, int download);

    /* Abort the sync.  Called from the main thread */
    void abort();

    bool isSyncRunning() const { return _syncRunning; }

    /* Set the maximum size a folder can have without asking for confirmation
     * -1 means infinite
     */
    void setNewBigFolderSizeLimit(qint64 limit) { _newBigFolderSizeLimit = limit; }
    bool ignoreHiddenFiles() const { return _csync_ctx->ignore_hidden_files; }
    void setIgnoreHiddenFiles(bool ignore) { _csync_ctx->ignore_hidden_files = ignore; }

    ExcludedFiles &excludedFiles() { return *_excludedFiles; }
    Utility::StopWatch &stopWatch() { return _stopWatch; }
    SyncFileStatusTracker &syncFileStatusTracker() { return *_syncFileStatusTracker; }

    /* Return true if we detected that another sync is needed to complete the sync */
    bool isAnotherSyncNeeded() { return _anotherSyncNeeded; }

    /** Get the ms since a file was touched, or -1 if it wasn't.
     *
     * Thread-safe.
     */
    qint64 timeSinceFileTouched(const QString& fn) const;

    AccountPtr account() const;
    SyncJournalDb *journal() const { return _journal; }
    QString localPath() const { return _localPath; }
    /**
     * Minimum age, in milisecond, of a file that can be uploaded.
     * Files more recent than that are not going to be uploaeded as they are considered
     * too young and possibly still changing
     */
    static qint64 minimumFileAgeForUpload; // in ms

signals:
    void csyncError( const QString& );
    void csyncUnavailable();

    // During update, before reconcile
    void rootEtag(QString);
    void folderDiscovered(bool local, const QString &folderUrl);

    // before actual syncing (after update+reconcile) for each item
    void syncItemDiscovered(const SyncFileItem&);
    // after the above signals. with the items that actually need propagating
    void aboutToPropagate(SyncFileItemVector&);

    // after each item completed by a job (successful or not)
    void itemCompleted(const SyncFileItem&, const PropagatorJob&);

    // after sync is done
    void treeWalkResult(const SyncFileItemVector&);

    void transmissionProgress( const ProgressInfo& progress );

    void finished(bool success);
    void started();

    /**
     * Emited when the sync engine detects that all the files have been removed or change.
     * This usually happen when the server was reset or something.
     * Set *cancel to true in a slot connected from this signal to abort the sync.
     */
    void aboutToRemoveAllFiles(SyncFileItem::Direction direction, bool *cancel);
    /**
     * Emited when the sync engine detects that all the files are changed to dates in the past.
     * This usually happen when a backup was restored on the server from an earlier date.
     * Set *restore to true in a slot connected from this signal to re-upload all files.
     */
    void aboutToRestoreBackup(bool *restore);

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder);

    /** Emitted when propagation has problems with a locked file.
     *
     * Forwarded from OwncloudPropagator::seenLockedFile.
     */
    void seenLockedFile(const QString &fileName);

private slots:
    void slotRootEtagReceived(const QString &);
    void slotItemCompleted(const SyncFileItem& item, const PropagatorJob & job);
    void slotFinished(bool success);
    void slotProgress(const SyncFileItem& item, quint64 curent);
    void slotDiscoveryJobFinished(int updateResult);
    void slotCleanPollsJobAborted(const QString &error);

    /** Records that a file was touched by a job. */
    void slotAddTouchedFile(const QString& fn);

    /** Wipes the _touchedFiles hash */
    void slotClearTouchedFiles();

private:
    void handleSyncError(CSYNC *ctx, const char *state);

    QString journalDbFilePath() const;

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    bool checkErrorBlacklisting( SyncFileItem &item );

    // Cleans up unnecessary downloadinfo entries in the journal as well
    // as their temporary files.
    void deleteStaleDownloadInfos();

    // Removes stale uploadinfos from the journal.
    void deleteStaleUploadInfos();

    // Removes stale error blacklist entries from the journal.
    void deleteStaleErrorBlacklistEntries();

    // cleanup and emit the finished signal
    void finalize(bool success);

    static bool s_anySyncRunning; //true when one sync is running somewhere (for debugging)

    // Must only be acessed during update and reconcile
    QMap<QString, SyncFileItemPtr> _syncItemMap;

    // should be called _syncItems (present tense). It's the items from the _syncItemMap but
    // sorted and re-adjusted based on permissions.
    SyncFileItemVector _syncedItems;

    AccountPtr _account;
    CSYNC *_csync_ctx;
    bool _needsUpdate;
    bool _syncRunning;
    QString _localPath;
    QString _remotePath;
    QString _remoteRootEtag;
    SyncJournalDb *_journal;
    QPointer<DiscoveryMainThread> _discoveryMainThread;
    QSharedPointer <OwncloudPropagator> _propagator;

    // After a sync, only the syncdb entries whose filenames appear in this
    // set will be kept. See _temporarilyUnavailablePaths.
    QSet<QString> _seenFiles;

    // Some paths might be temporarily unavailable on the server, for
    // example due to 503 Storage not available. Deleting information
    // about the files from the database in these cases would lead to
    // incorrect synchronization.
    // Therefore all syncdb entries whose filename starts with one of
    // the paths in this set will be kept.
    // The specific case that fails otherwise is deleting a local file
    // while the remote says storage not available.
    QSet<QString> _temporarilyUnavailablePaths;

    QThread _thread;

    QScopedPointer<ProgressInfo> _progressInfo;

    QScopedPointer<ExcludedFiles> _excludedFiles;
    QScopedPointer<SyncFileStatusTracker> _syncFileStatusTracker;
    Utility::StopWatch _stopWatch;

    // maps the origin and the target of the folders that have been renamed
    QHash<QString, QString> _renamedFolders;
    QString adjustRenamedPath(const QString &original);

    /**
     * check if we are allowed to propagate everything, and if we are not, adjust the instructions
     * to recover
     */
    void checkForPermission();
    QByteArray getPermissions(const QString& file) const;

    /**
     * Instead of downloading files from the server, upload the files to the server
     */
    void restoreOldFiles();

    bool _hasNoneFiles; // true if there is at least one file which was not changed on the server
    bool _hasRemoveFile; // true if there is at leasr one file with instruction REMOVE
    bool _hasForwardInTimeFiles; // true if there is at least one file from the server that goes forward in time
    int _backInTimeFiles; // number of files which goes back in time from the server


    int _uploadLimit;
    int _downloadLimit;
    /* maximum size a folder can have without asking for confirmation: -1 means infinite */
    qint64 _newBigFolderSizeLimit;

    // hash containing the permissions on the remote directory
    QHash<QString, QByteArray> _remotePerms;

    /// Hook for computing checksums from csync_update
    CSyncChecksumHook _checksum_hook;

    bool _anotherSyncNeeded;

    /** Stores the time since a job touched a file. */
    QHash<QString, QElapsedTimer> _touchedFiles;

    /** For clearing the _touchedFiles variable after sync finished */
    QTimer _clearTouchedFilesTimer;
};

}

#endif // CSYNCTHREAD_H
