/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

OCSYNC_EXPORT Q_NAMESPACE

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
        /// Emitted once at start
        Starting,

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

    [[nodiscard]] Status status() const;

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
    [[nodiscard]] bool isUpdatingEstimates() const;

    /**
     * Increase the file and size totals by the amount indicated in item.
     */
    void adjustTotalsForFile(const SyncFileItem &item);

    [[nodiscard]] qint64 totalFiles() const;
    [[nodiscard]] qint64 completedFiles() const;

    [[nodiscard]] qint64 totalSize() const;
    [[nodiscard]] qint64 completedSize() const;

    /** Number of a file that is currently in progress. */
    [[nodiscard]] qint64 currentFile() const;

    /** Return true if the size needs to be taken in account in the total amount of time */
    static inline bool isSizeDependent(const SyncFileItem &item)
    {
        return !item.isDirectory()
            && (item._instruction == CSYNC_INSTRUCTION_CONFLICT
                || item._instruction == CSYNC_INSTRUCTION_SYNC
                || item._instruction == CSYNC_INSTRUCTION_NEW
                || item._instruction == CSYNC_INSTRUCTION_TYPE_CHANGE)
            && !(item._type == ItemTypeVirtualFile
                 || item._type == ItemTypeVirtualFileDehydration);
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
        /** Returns the estimates about progress per second and eta. */
        [[nodiscard]] Estimates estimates() const;

        [[nodiscard]] qint64 completed() const;
        [[nodiscard]] qint64 remaining() const;

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
        double _progressPerSec = 0;
        qint64 _prevCompleted = 0;

        // Used to get to a good value faster when
        // progress measurement stats. See update().
        double _initialSmoothing = 1.0;

        // Set and updated by ProgressInfo
        qint64 _completed = 0;
        qint64 _total = 0;

        friend class ProgressInfo;
    };

    Status _status = Starting;

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
    [[nodiscard]] Estimates totalProgress() const;

    /**
     * Get the optimistic eta.
     *
     * This value is based on the highest observed transfer bandwidth
     * and files-per-second speed.
     */
    [[nodiscard]] quint64 optimisticEta() const;

    /**
     * Whether the remaining-time estimate is trusted.
     *
     * We don't trust it if it is hugely above the optimistic estimate.
     * See #5046.
     */
    [[nodiscard]] bool trustEta() const;

    /**
     * Get the current file completion estimate structure
     */
    [[nodiscard]] Estimates fileProgress(const SyncFileItem &item) const;

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
    qint64 _totalSizeOfCompletedJobs = 0LL;

    // The fastest observed rate of files per second in this sync.
    double _maxFilesPerSecond = 0.0;
    double _maxBytesPerSecond = 0.0;
};

namespace Progress {

    OWNCLOUDSYNC_EXPORT QString asActionString(const SyncFileItem &item);
    OWNCLOUDSYNC_EXPORT QString asResultString(const SyncFileItem &item);

    OWNCLOUDSYNC_EXPORT bool isWarningKind(SyncFileItem::Status);
    OWNCLOUDSYNC_EXPORT bool isIgnoredKind(SyncFileItem::Status);
}

/** Type of error
 *
 * Used for ProgressDispatcher::syncError. May trigger error interactivity
 * in IssuesWidget.
 */
enum class ErrorCategory {
    NoError,
    GenericError,
    NetworkError,
    InsufficientRemoteStorage,
};
Q_ENUM_NS(OCC::ErrorCategory)

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

    friend class Folder; // only allow Folder class to access the setting slots.
public:
    static ProgressDispatcher *instance();
    ~ProgressDispatcher() override;

signals:
    /**
      @brief Signals the progress of data transmission.

      @param[out]  folder The folder which is being processed
      @param[out]  progress   A struct with all progress info.

     */
    void progressInfo(const QString &folder, const OCC::ProgressInfo &progress);
    /**
     * @brief: the item was completed by a job
     */
    void itemCompleted(const QString &folder, const OCC::SyncFileItemPtr &item, const OCC::ErrorCategory category);

    /**
     * @brief A new folder-wide sync error was seen.
     */
    void syncError(const QString &folder, const QString &message, OCC::ErrorCategory category);

    /**
     * @brief Emitted when an error needs to be added into GUI
     * @param[out] folder The folder which is being processed
     * @param[out] status of the error
     * @param[out] full error message
     * @param[out] subject (optional)
     */
    void addErrorToGui(const QString &folder, const OCC::SyncFileItem::Status status, const QString &errorMessage, const QString &subject, const OCC::ErrorCategory category);

    /**
     * @brief Emitted for a folder when a sync is done, listing all pending conflicts
     */
    void folderConflicts(const QString &folder, const QStringList &conflictPaths);

protected:
    void setProgressInfo(const QString &folder, const ProgressInfo &progress);

private:
    ProgressDispatcher(QObject *parent = nullptr);

    QElapsedTimer _timer;
    static ProgressDispatcher *_instance;
};
}
#endif // PROGRESSDISPATCHER_H
