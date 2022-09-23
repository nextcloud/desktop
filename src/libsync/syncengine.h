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

#pragma once

#include <cstdint>

#include <QMutex>
#include <QThread>
#include <QString>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QSharedPointer>
#include <set>

#include "syncfileitem.h"
#include "progressdispatcher.h"
#include "common/utility.h"
#include "syncfilestatustracker.h"
#include "accountfwd.h"
#include "discoveryphase.h"
#include "common/checksums.h"

class QProcess;

namespace OCC {

class SyncJournalFileRecord;
class SyncJournalDb;
class OwncloudPropagator;
class ProcessDirectoryJob;

enum AnotherSyncNeeded {
    NoFollowUpSync,
    ImmediateFollowUp, // schedule this again immediately (limited amount of times)
    DelayedFollowUp // regularly schedule this folder again (around 1/minute, unlimited)
};

/**
 * @brief The SyncEngine class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncEngine : public QObject
{
    Q_OBJECT
public:
    SyncEngine(AccountPtr account,
               const QString &localPath,
               const SyncOptions &syncOptions,
               const QString &remotePath,
               SyncJournalDb *journal);

    ~SyncEngine() override;

    Q_INVOKABLE void startSync();
    void setNetworkLimits(int upload, int download);

    /* Abort the sync.  Called from the main thread */
    void abort();

    bool isSyncRunning() const { return _syncRunning; }

    SyncOptions syncOptions() const { return _syncOptions; }
    void setSyncOptions(const SyncOptions &options) { _syncOptions = options; }
    bool ignoreHiddenFiles() const { return _ignore_hidden_files; }
    void setIgnoreHiddenFiles(bool ignore) { _ignore_hidden_files = ignore; }

    void addAcceptedInvalidFileName(const QString& filePath);

    ExcludedFiles &excludedFiles() { return *_excludedFiles; }
    Utility::StopWatch &stopWatch() { return _stopWatch; }
    SyncFileStatusTracker &syncFileStatusTracker() { return *_syncFileStatusTracker; }

    /* Returns whether another sync is needed to complete the sync */
    AnotherSyncNeeded isAnotherSyncNeeded() { return _anotherSyncNeeded; }

    bool wasFileTouched(const QString &fn) const;

    AccountPtr account() const;
    SyncJournalDb *journal() const { return _journal; }
    QString localPath() const { return _localPath; }

    /** Duration in ms that uploads should be delayed after a file change
     *
     * In certain situations a file can be written to very regularly over a large
     * amount of time. Copying a large file could take a while. A logfile could be
     * updated every second.
     *
     * In these cases it isn't desirable to attempt to upload the "unfinished" file.
     * To avoid that, uploads of files where the distance between the mtime and the
     * current time is less than this duration are skipped.
     */
    static std::chrono::milliseconds minimumFileAgeForUpload;

    /**
     * Control whether local discovery should read from filesystem or db.
     *
     * If style is DatabaseAndFilesystem, paths a set of file paths relative to
     * the synced folder. All the parent directories of these paths will not
     * be read from the db and scanned on the filesystem.
     *
     * Note, the style and paths are only retained for the next sync and
     * revert afterwards. Use _lastLocalDiscoveryStyle to discover the last
     * sync's style.
     */
    void setLocalDiscoveryOptions(LocalDiscoveryStyle style, std::set<QString> paths = {});

    /**
     * Returns whether the given folder-relative path should be locally discovered
     * given the local discovery options.
     *
     * Example: If path is 'foo/bar' and style is DatabaseAndFilesystem and dirs contains
     *     'foo/bar/touched_file', then the result will be true.
     */
    bool shouldDiscoverLocally(const QString &path) const;

    /** Access the last sync run's local discovery style */
    LocalDiscoveryStyle lastLocalDiscoveryStyle() const { return _lastLocalDiscoveryStyle; }

    /** Removes all virtual file db entries and dehydrated local placeholders.
     *
     * Particularly useful when switching off vfs mode or switching to a
     * different kind of vfs.
     *
     * Note that *hydrated* placeholder files might still be left. These will
     * get cleaned up by Vfs::unregisterFolder().
     */
    static void wipeVirtualFiles(const QString &localPath, SyncJournalDb &journal, Vfs &vfs);

    static void switchToVirtualFiles(const QString &localPath, SyncJournalDb &journal, Vfs &vfs);

    auto getPropagator() { return _propagator; } // for the test

signals:
    // During update, before reconcile
    void rootEtag(const QByteArray &, const QDateTime &);

    // after the above signals. with the items that actually need propagating
    void aboutToPropagate(SyncFileItemVector &);

    // after each item completed by a job (successful or not)
    void itemCompleted(const SyncFileItemPtr &);

    void transmissionProgress(const ProgressInfo &progress);

    /// We've produced a new sync error of a type.
    void syncError(const QString &message, ErrorCategory category = ErrorCategory::Normal);

    void addErrorToGui(SyncFileItem::Status status, const QString &errorMessage, const QString &subject);

    void finished(bool success);
    void started();

    /**
     * Emited when the sync engine detects that all the files have been removed or change.
     * This usually happen when the server was reset or something.
     * Set *cancel to true in a slot connected from this signal to abort the sync.
     */
    void aboutToRemoveAllFiles(SyncFileItem::Direction direction, std::function<void(bool)> f);

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);

    /** Emitted when propagation has problems with a locked file.
     *
     * Forwarded from OwncloudPropagator::seenLockedFile.
     */
    void seenLockedFile(const QString &fileName);

private slots:
    void slotFolderDiscovered(bool local, const QString &folder);
    void slotRootEtagReceived(const QByteArray &, const QDateTime &time);

    /** When the discovery phase discovers an item */
    void slotItemDiscovered(const SyncFileItemPtr &item);

    /** Called when a SyncFileItem gets accepted for a sync.
     *
     * Mostly done in initial creation inside treewalkFile but
     * can also be called via the propagator for items that are
     * created during propagation.
     */
    void slotNewItem(const SyncFileItemPtr &item);

    void slotItemCompleted(const SyncFileItemPtr &item);
    void slotDiscoveryFinished();
    void slotPropagationFinished(bool success);
    void slotProgress(const SyncFileItem &item, qint64 curent);
    void slotCleanPollsJobAborted(const QString &error);

    /** Records that a file was touched by a job. */
    void slotAddTouchedFile(const QString &fn);

    /** Wipes the _touchedFiles hash */
    void slotClearTouchedFiles();

    /** Emit a summary error, unless it was seen before */
    void slotSummaryError(const QString &message);

    void slotInsufficientLocalStorage();
    void slotInsufficientRemoteStorage();

    void slotScheduleFilesDelayedSync();
    void slotCleanupScheduledSyncTimers();

private:
    // Some files need a sync run to be executed at a specified time after
    // their status is scheduled to change (e.g. lock status will expire in
    // 20 minutes.)
    //
    // Rather than execute a sync run for each file that needs one, we want
    // to schedule as few sync runs as possible, trying to have the state of
    // these files updated in a timely manner without scheduling runs too
    // frequently. We can therefore group files into a bucket.
    //
    // A bucket contains a group of files requiring a sync run in close
    // proximity to each other, with an assigned sync timer interval that can
    // be used to schedule a sync run which will update all the files in the
    // bucket at the time their state is scheduled to change.
    //
    // In the pair, first is the actual time at which the bucket is going to
    // have its sync scheduled. Second is the vector of all the (paths of)
    // files that fall into this bucket.
    //
    // See SyncEngine::groupNeededScheduledSyncRuns and
    // SyncEngine::slotScheduleFilesDelayedSync for usage.
    struct ScheduledSyncBucket {
        long long scheduledSyncTimerSecs;
        QVector<QString> files;
    };

    bool checkErrorBlacklisting(SyncFileItem &item);

    // Cleans up unnecessary downloadinfo entries in the journal as well
    // as their temporary files.
    void deleteStaleDownloadInfos(const SyncFileItemVector &syncItems);

    // Removes stale uploadinfos from the journal.
    void deleteStaleUploadInfos(const SyncFileItemVector &syncItems);

    // Removes stale error blacklist entries from the journal.
    void deleteStaleErrorBlacklistEntries(const SyncFileItemVector &syncItems);

    // Removes stale and adds missing conflict records after sync
    void conflictRecordMaintenance();

    // cleanup and emit the finished signal
    void finalize(bool success);

    // Aggregate scheduled sync runs into interval buckets. Can be used to
    // schedule a sync run per bucket instead of per file, reducing load.
    //
    // Bucket classification is done by simply dividing the seconds until
    // scheduled sync time by the interval (note -- integer division!)
    QHash<long long, ScheduledSyncBucket> groupNeededScheduledSyncRuns(const int interval);

    // Checks if there is already a scheduled sync run timer active near the
    // time provided as the parameter.
    //
    // If this timer will expire within the interval provided, the return is
    // true.
    //
    // If this expiration occurs before the scheduled sync run provided as the
    // parameter, it is rescheduled to expire at the time of the parameter.
    bool nearbyScheduledSyncTimerUsable(const long long scheduledSyncTimerSecs,
                                        const long long intervalSecs) const;

    static bool s_anySyncRunning; //true when one sync is running somewhere (for debugging)

    // Must only be acessed during update and reconcile
    QVector<SyncFileItemPtr> _syncItems;

    AccountPtr _account;
    bool _needsUpdate;
    bool _syncRunning;
    QString _localPath;
    QString _remotePath;
    QByteArray _remoteRootEtag;
    SyncJournalDb *_journal;
    QScopedPointer<DiscoveryPhase> _discoveryPhase;
    QSharedPointer<OwncloudPropagator> _propagator;

    QSet<QString> _bulkUploadBlackList;

    // List of all files with conflicts
    QSet<QString> _seenConflictFiles;

    QScopedPointer<ProgressInfo> _progressInfo;

    QScopedPointer<ExcludedFiles> _excludedFiles;
    QScopedPointer<SyncFileStatusTracker> _syncFileStatusTracker;
    Utility::StopWatch _stopWatch;

    /**
     * check if we are allowed to propagate everything, and if we are not, adjust the instructions
     * to recover
     */
    void checkForPermission(SyncFileItemVector &syncItems);
    RemotePermissions getPermissions(const QString &file) const;

    /**
     * Instead of downloading files from the server, upload the files to the server
     */
    void restoreOldFiles(SyncFileItemVector &syncItems);

    // true if there is at least one file which was not changed on the server
    bool _hasNoneFiles;

    // true if there is at leasr one file with instruction REMOVE
    bool _hasRemoveFile;

    // If ignored files should be ignored
    bool _ignore_hidden_files = false;


    int _uploadLimit;
    int _downloadLimit;
    SyncOptions _syncOptions;

    AnotherSyncNeeded _anotherSyncNeeded;

    /** Stores the time since a job touched a file. */
    QMultiMap<QElapsedTimer, QString> _touchedFiles;

    QElapsedTimer _lastUpdateProgressCallbackCall;

    /** For clearing the _touchedFiles variable after sync finished */
    QTimer _clearTouchedFilesTimer;

    /** List of unique errors that occurred in a sync run. */
    QSet<QString> _uniqueErrors;

    /** The kind of local discovery the last sync run used */
    LocalDiscoveryStyle _lastLocalDiscoveryStyle = LocalDiscoveryStyle::FilesystemOnly;
    LocalDiscoveryStyle _localDiscoveryStyle = LocalDiscoveryStyle::FilesystemOnly;
    std::set<QString> _localDiscoveryPaths;

    QStringList _leadingAndTrailingSpacesFilesAllowed;

    // Hash of files we have scheduled for later sync runs, along with their lock expire times
    // NOTE: not necessarily the time at which their sync run will take place
    QHash<QString, long long> _filesScheduledForLaterSync;
    QVector<QSharedPointer<QTimer>> _scheduledSyncTimers;
};
}

