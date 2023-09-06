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

#ifndef PROGRESSDISPATCHER_H
#define PROGRESSDISPATCHER_H

#include "owncloudlib.h"
#include <QObject>
#include <QHash>
#include <QTime>
#include <QQueue>
#include <QElapsedTimer>
#include <QTimer>

#include "syncfileitem.h"

namespace OCC {
class Folder;
/**
 * @brief The ProgressInfo class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ProgressInfo : public QObject
{
    Q_OBJECT
public:
    ProgressInfo();

    /** Resets for a new sync run.
     */
    void reset();

    /** Records the status of the sync run
     */
    enum Status {
        None,

        /**
         * Emitted once without _currentDiscoveredFolder when it starts,
         * then for each folder.
         */
        Discovery,

        /// Emitted once when reconcile starts
        Reconcile,

        /// Emitted during propagation, with progress data
        Propagation,

        /**
         * Emitted once when done
         *
         * Except when SyncEngine jumps directly to finalize() without going
         * through slotPropagationFinished().
         */
        Done
    };
    Q_ENUM(Status);

    Status status() const;

    /**
     * Called when propagation starts.
     *
     * isUpdatingEstimates() will return true afterwards.
     */
    void startEstimateUpdates();

    /**
     * Returns true when startEstimateUpdates() was called.
     *
     * This is used when the SyncEngine wants to indicate a new sync
     * is about to start via the transmissionProgress() signal. The
     * first ProgressInfo will have isUpdatingEstimates() == false.
     */
    bool isUpdatingEstimates() const;

    /**
     * Increase the file and size totals by the amount indicated in item.
     */
    void adjustTotalsForFile(const SyncFileItem &item);

    /**
     * Update totals for item.
     *
     * adjustTotalsForFile() is called during the treewalk phase to collect
     * the initial total size and file count.
     * This function is called at most once per item during propagation
     * to adjust them when its actual size has been determined.
     *
     * The value in item.size must be the same as during the call to
     * adjustTotalsForFile() while newSize is the newly determined actual
     * size.
     */
    void updateTotalsForFile(const SyncFileItem &item, qint64 newSize);

    qint64 totalFiles() const;
    qint64 completedFiles() const;

    qint64 totalSize() const;
    qint64 completedSize() const;

    /** Number of a file that is currently in progress. */
    qint64 currentFile() const;

    /** Return true if the size needs to be taken in account in the total amount of time */
    static inline bool isSizeDependent(const SyncFileItem &item)
    {
        return !item.isDirectory()
            && (item.instruction() & (CSYNC_INSTRUCTION_CONFLICT | CSYNC_INSTRUCTION_SYNC | CSYNC_INSTRUCTION_NEW | CSYNC_INSTRUCTION_TYPE_CHANGE))
            && !(item._type == ItemTypeVirtualFile || item._type == ItemTypeVirtualFileDehydration);
    }

    /**
     * Holds estimates about progress, returned to the user.
     */
    struct Estimates
    {
        /// Estimated completion amount per second. (of bytes or files)
        qint64 estimatedBandwidth;

        /// Estimated time remaining in milliseconds.
        quint64 estimatedEta;
    };

    /**
     * Holds the current state of something making progress and maintains an
     * estimate of the current progress per second.
     */
    struct OWNCLOUDSYNC_EXPORT Progress
    {
        Progress()
            : _progressPerSec(0)
            , _prevCompleted(0)
            , _initialSmoothing(1.0)
            , _completed(0)
            , _total(0)
        {
        }

        /** Returns the estimates about progress per second and eta. */
        Estimates estimates() const;

        qint64 completed() const;
        qint64 remaining() const;

    private:
        /**
         * Update the exponential moving average estimate of _progressPerSec.
         */
        void update();

        /**
         * Changes the _completed value and does sanity checks on
         * _prevCompleted and _total.
         */
        void setCompleted(qint64 completed);

        // Updated by update()
        double _progressPerSec;
        qint64 _prevCompleted;

        // Used to get to a good value faster when
        // progress measurement stats. See update().
        double _initialSmoothing;

        // Set and updated by ProgressInfo
        qint64 _completed;
        qint64 _total;

        friend class ProgressInfo;
    };

    Status _status;

    struct OWNCLOUDSYNC_EXPORT ProgressItem
    {
        SyncFileItem _item;
        Progress _progress;
    };
    QHash<QString, ProgressItem> _currentItems;

    SyncFileItem _lastCompletedItem;

    // Used during local and remote update phase
    QString _currentDiscoveredRemoteFolder;
    QString _currentDiscoveredLocalFolder;

    void setProgressComplete(const SyncFileItem &item);

    void setProgressItem(const SyncFileItem &item, qint64 completed);

    /**
     * Get the total completion estimate
     */
    Estimates totalProgress() const;

    /**
     * Get the optimistic eta.
     *
     * This value is based on the highest observed transfer bandwidth
     * and files-per-second speed.
     */
    quint64 optimisticEta() const;

    /**
     * Whether the remaining-time estimate is trusted.
     *
     * We don't trust it if it is hugely above the optimistic estimate.
     * See #5046.
     */
    bool trustEta() const;

    /**
     * Get the current file completion estimate structure
     */
    Estimates fileProgress(const SyncFileItem &item) const;

private slots:
    /**
     * Called every second once started, this function updates the
     * estimates.
     */
    void updateEstimates();

private:
    // Sets the completed size by summing finished jobs with the progress
    // of active ones.
    void recomputeCompletedSize();

    // Triggers the update() slot every second once propagation started.
    QTimer _updateEstimatesTimer;

    Progress _sizeProgress;
    Progress _fileProgress;

    // All size from completed jobs only.
    qint64 _totalSizeOfCompletedJobs;

    // The fastest observed rate of files per second in this sync.
    double _maxFilesPerSecond;
    double _maxBytesPerSecond;
};

namespace Progress {

    OWNCLOUDSYNC_EXPORT QString asActionString(const SyncFileItem &item);
    OWNCLOUDSYNC_EXPORT QString asResultString(const SyncFileItem &item);

    OWNCLOUDSYNC_EXPORT bool isWarningKind(SyncFileItem::Status);
    OWNCLOUDSYNC_EXPORT bool isIgnoredKind(SyncFileItem::Status);
}

// work around for only having one namespace OCC, and this enum not beeing in a QObject
namespace ErrorCategoryPrivate {
    OWNCLOUDSYNC_EXPORT Q_NAMESPACE;

    /** Type of error
     *
     * Used for ProgressDispatcher::syncError. May trigger error interactivity
     * in IssuesWidget.
     */
    enum class ErrorCategory {
        Normal,
        InsufficientRemoteStorage,
    };

    Q_ENUM_NS(ErrorCategory);
}
using ErrorCategoryPrivate::ErrorCategory;

/**
 * @file progressdispatcher.h
 * @brief A singleton class to provide sync progress information to other gui classes.
 *
 * How to use the ProgressDispatcher:
 * Just connect to the two signals either to progress for every individual file
 * or the overall sync progress.
 *
 */
class OWNCLOUDSYNC_EXPORT ProgressDispatcher : public QObject
{
    Q_OBJECT
public:
    static ProgressDispatcher *instance();
    ~ProgressDispatcher() override;

signals:
    /**
      @brief Signals the progress of data transmission.

      @param[out]  folder The folder which is being processed
      @param[out]  progress   A struct with all progress info.

     */
    void progressInfo(Folder *folder, const ProgressInfo &progress);
    /**
     * @brief: the item was completed by a job
     */
    void itemCompleted(Folder *folder, const SyncFileItemPtr &item);

    /**
     * @brief A new folder-wide sync error was seen.
     */
    void syncError(Folder *folder, const QString &message, ErrorCategory category);

    void excluded(Folder *folder, const QString &path);


    /**
     * @brief Emitted for a folder when a sync is done, listing all pending conflicts
     */
    void folderConflicts(Folder *folder, const QStringList &conflictPaths);

private:
    ProgressDispatcher(QObject *parent = nullptr);

    QElapsedTimer _timer;
    static ProgressDispatcher *_instance;
};
}
#endif // PROGRESSDISPATCHER_H
