/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OWNCLOUDPROPAGATOR_H
#define OWNCLOUDPROPAGATOR_H

#include <QHash>
#include <QObject>
#include <QMap>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QIODevice>
#include <QMutex>
#include <QNetworkReply>

#include "accountfwd.h"
#include "bandwidthmanager.h"
#include "csync.h"
#include "progressdispatcher.h"
#include "syncfileitem.h"
#include "syncoptions.h"

#include "common/syncjournaldb.h"
#include "common/utility.h"
#include "common/vfs.h"

#include <deque>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcPropagator)

/** Free disk space threshold below which syncs will abort and not even start.
 */
qint64 criticalFreeSpaceLimit();

/** The client will not intentionally reduce the available free disk space below
 *  this limit.
 *
 * Uploads will still run and downloads that are small enough will continue too.
 */
qint64 freeSpaceLimit();

void blacklistUpdate(SyncJournalDb *journal, SyncFileItem &item);

class SyncJournalDb;
class OwncloudPropagator;
class PropagatorCompositeJob;
class FolderMetadata;

/**
 * @brief the base class of propagator jobs
 *
 * This can either be a job, or a container for jobs.
 * If it is a composite job, it then inherits from PropagateDirectory
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT PropagatorJob : public QObject
{
    Q_OBJECT

public:
    explicit PropagatorJob(OwncloudPropagator *propagator);

    enum AbortType {
        Synchronous,
        Asynchronous
    };

    Q_ENUM(AbortType)

    enum JobState {
        NotYetStarted,
        Running,
        Finished
    };
    JobState _state = NotYetStarted;

    Q_ENUM(JobState)

    enum JobParallelism {

        /** Jobs can be run in parallel to this job */
        FullParallelism,

        /** No other job shall be started until this one has finished.
            So this job is guaranteed to finish before any jobs below it
            are executed. */
        WaitForFinished,
    };

    Q_ENUM(JobParallelism)

    [[nodiscard]] virtual JobParallelism parallelism() const { return FullParallelism; }

    /**
     * For "small" jobs
     */
    virtual bool isLikelyFinishedQuickly() { return false; }

    /** The space that the running jobs need to complete but don't actually use yet.
     *
     * Note that this does *not* include the disk space that's already
     * in use by running jobs for things like a download-in-progress.
     */
    [[nodiscard]] virtual qint64 committedDiskSpace() const { return 0; }

    /** Set the associated composite job
     *
     * Used only from PropagatorCompositeJob itself, when a job is added
     * and from PropagateDirectory to associate the subJobs with the first
     * job.
     */
    void setAssociatedComposite(PropagatorCompositeJob *job) { _associatedComposite = job; }

public slots:
    /*
     * Asynchronous abort requires emit of abortFinished() signal,
     * while synchronous is expected to abort immedietaly.
    */
    virtual void abort(OCC::PropagatorJob::AbortType abortType) {
        if (abortType == AbortType::Asynchronous)
            emit abortFinished();
    }

    /** Starts this job, or a new subjob
     * returns true if a job was started.
     */
    virtual bool scheduleSelfOrChild() = 0;
signals:
    /**
     * Emitted when the job is fully finished
     */
    void finished(OCC::SyncFileItem::Status);

    /**
     * Emitted when the abort is fully finished
     */
    void abortFinished(OCC::SyncFileItem::Status status = SyncFileItem::NormalError);
protected:
    [[nodiscard]] OwncloudPropagator *propagator() const;

    static ErrorCategory errorCategoryFromNetworkError(const QNetworkReply::NetworkError error);

    /** If this job gets added to a composite job, this will point to the parent.
     *
     * For the PropagateDirectory::_firstJob it will point to
     * PropagateDirectory::_subJobs.
     *
     * That can be useful for jobs that want to spawn follow-up jobs without
     * becoming composite jobs themselves.
     */
    PropagatorCompositeJob *_associatedComposite = nullptr;
};

/*
 * Abstract class to propagate a single item
 */
class PropagateItemJob : public PropagatorJob
{
    Q_OBJECT
protected:
    virtual void done(const SyncFileItem::Status status, const QString &errorString, const ErrorCategory category);

    /*
     * set a custom restore job message that is used if the restore job succeeded.
     * It is displayed in the activity view.
     */
    [[nodiscard]] QString restoreJobMsg() const
    {
        return _item->_isRestoration ? _item->_errorString : QString();
    }
    void setRestoreJobMsg(const QString &msg = QString())
    {
        _item->_isRestoration = true;
        _item->_errorString = msg;
    }

    [[nodiscard]] bool hasEncryptedAncestor() const;

protected slots:
    void slotRestoreJobFinished(OCC::SyncFileItem::Status status);

private:
    void reportClientStatuses();

    QScopedPointer<PropagateItemJob> _restoreJob;
    JobParallelism _parallelism = FullParallelism;

public:
    PropagateItemJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagatorJob(propagator)
        , _item(item)
    {
        // we should always execute jobs that process the E2EE API calls as sequential jobs
        // TODO: In fact, we must make sure Lock/Unlock are not colliding and always wait for each other to complete. So, we could refactor this "_parallelism" later
        // so every "PropagateItemJob" that will potentially execute Lock job on E2EE folder will get executed sequentially.
        // As an alternative, we could optimize Lock/Unlock calls, so we do a batch-write on one folder and only lock and unlock a folder once per batch.
        _parallelism = (_item->isEncrypted() || hasEncryptedAncestor()) ? WaitForFinished : FullParallelism;
    }
    ~PropagateItemJob() override;

    bool scheduleSelfOrChild() override
    {
        if (_state != NotYetStarted) {
            return false;
        }
        qCInfo(lcPropagator) << "Starting" << _item->_instruction << "propagation of" << _item->destination() << "by" << this;

        _state = Running;
        QMetaObject::invokeMethod(this, "start"); // We could be in a different thread (neon jobs)
        return true;
    }

    [[nodiscard]] JobParallelism parallelism() const override { return _parallelism; }

    SyncFileItemPtr _item;

public slots:
    virtual void start() = 0;
};

/**
 * @brief Job that runs subjobs. It becomes finished only when all subjobs are finished.
 * @ingroup libsync
 */
class PropagatorCompositeJob : public PropagatorJob
{
    Q_OBJECT
public:
    QVector<PropagatorJob *> _jobsToDo;
    SyncFileItemVector _tasksToDo;
    QVector<PropagatorJob *> _runningJobs;
    SyncFileItem::Status _hasError = SyncFileItem::NoStatus; // NoStatus,  or NormalError / SoftError if there was an error
    quint64 _abortsCount = 0;
    bool _isAnyCaseClashChild = false;
    bool _isAnyInvalidCharChild = false;

    explicit PropagatorCompositeJob(OwncloudPropagator *propagator)
        : PropagatorJob(propagator)
    {
    }

    // Don't delete jobs in _jobsToDo and _runningJobs: they have parents
    // that will be responsible for cleanup. Deleting them here would risk
    // deleting something that has already been deleted by a shared parent.
    ~PropagatorCompositeJob() override = default;

    void appendJob(PropagatorJob *job);
    void appendTask(const SyncFileItemPtr &item)
    {
        _tasksToDo.append(item);
    }

    bool scheduleSelfOrChild() override;
    [[nodiscard]] JobParallelism parallelism() const override;

    /*
     * Abort synchronously or asynchronously - some jobs
     * require to be finished without immediete abort (abort on job might
     * cause conflicts/duplicated files - owncloud/client/issues/5949)
     */
    void abort(PropagatorJob::AbortType abortType) override
    {
        if (!_runningJobs.empty()) {
            _abortsCount = _runningJobs.size();
            for (const auto j : std::as_const(_runningJobs)) {
                if (abortType == AbortType::Asynchronous) {
                    connect(j, &PropagatorJob::abortFinished,
                            this, &PropagatorCompositeJob::slotSubJobAbortFinished);
                }
                j->abort(abortType);
            }
        } else if (abortType == AbortType::Asynchronous){
            emit abortFinished();
        }
    }

    [[nodiscard]] qint64 committedDiskSpace() const override;

private slots:
    void slotSubJobAbortFinished();
    bool possiblyRunNextJob(OCC::PropagatorJob *next)
    {
        if (next->_state == NotYetStarted) {
            connect(next, &PropagatorJob::finished, this, &PropagatorCompositeJob::slotSubJobFinished);
        }
        return next->scheduleSelfOrChild();
    }

    void slotSubJobFinished(OCC::SyncFileItem::Status status);
    void finalize();
};

/**
 * @brief Propagate a directory, and all its sub entries.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT PropagateDirectory : public PropagatorJob
{
    Q_OBJECT
public:
    SyncFileItemPtr _item;
    // e.g: create the directory
    std::unique_ptr<PropagateItemJob> _firstJob;

    PropagatorCompositeJob _subJobs;

    explicit PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItemPtr &item);

    void appendJob(PropagatorJob *job)
    {
        _subJobs.appendJob(job);
    }

    void appendTask(const SyncFileItemPtr &item)
    {
        _subJobs.appendTask(item);
    }

    void willDeleteItemToClientTrashBin(const SyncFileItemPtr &item);

    bool scheduleSelfOrChild() override;
    [[nodiscard]] JobParallelism parallelism() const override;
    void abort(PropagatorJob::AbortType abortType) override
    {
        if (_firstJob)
            // Force first job to abort synchronously
            // even if caller allows async abort (asyncAbort)
            _firstJob->abort(AbortType::Synchronous);

        if (abortType == AbortType::Asynchronous){
            connect(&_subJobs, &PropagatorCompositeJob::abortFinished, this, &PropagateDirectory::abortFinished);
        }
        _subJobs.abort(abortType);
    }

    void increaseAffectedCount()
    {
        _firstJob->_item->_affectedItems++;
    }


    [[nodiscard]] qint64 committedDiskSpace() const override
    {
        return _subJobs.committedDiskSpace();
    }

private slots:

    void slotFirstJobFinished(OCC::SyncFileItem::Status status);
    virtual void slotSubJobsFinished(OCC::SyncFileItem::Status status);

};

/**
 * @brief Propagate the root directory, and all its sub entries.
 * @ingroup libsync
 *
 * Primary difference to PropagateDirectory is that it keeps track of directory
 * deletions that must happen at the very end.
 */
class OWNCLOUDSYNC_EXPORT PropagateRootDirectory : public PropagateDirectory
{
    Q_OBJECT
public:
    explicit PropagateRootDirectory(OwncloudPropagator *propagator);

    bool scheduleSelfOrChild() override;
    [[nodiscard]] JobParallelism parallelism() const override;
    void abort(PropagatorJob::AbortType abortType) override;

    [[nodiscard]] qint64 committedDiskSpace() const override;

public slots:
    void appendDirDeletionJob(OCC::PropagatorJob *job);

private slots:
    void slotSubJobsFinished(OCC::SyncFileItem::Status status) override;
    void slotDirDeletionJobsFinished(OCC::SyncFileItem::Status status);

private:
    bool scheduleDelayedJobs();

    PropagatorCompositeJob _dirDeletionJobs;

    SyncFileItem::Status _errorStatus = SyncFileItem::Status::NoStatus;
};

/**
 * @brief Dummy job that just mark it as completed and ignored
 * @ingroup libsync
 */
class PropagateIgnoreJob : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateIgnoreJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() override;
};

class PropagateVfsUpdateMetadataJob : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateVfsUpdateMetadataJob(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() override;
};

class PropagateUploadFileCommon;

class OWNCLOUDSYNC_EXPORT OwncloudPropagator : public QObject
{
    Q_OBJECT
public:
    SyncJournalDb *const _journal;
    bool _finishedEmited = false; // used to ensure that finished is only emitted once

public:
    OwncloudPropagator(AccountPtr account, const QString &localDir, const QString &remoteFolder, SyncJournalDb *progressDb, QSet<QString> &bulkUploadBlackList)
        : _journal(progressDb)
        , _bandwidthManager(this)
        , _chunkSize(10 * 1000 * 1000) // 10 MB, overridden in setSyncOptions
        , _account(account)
        , _localDir(Utility::trailingSlashPath(localDir))
        , _remoteFolder(Utility::trailingSlashPath(remoteFolder))
        , _bulkUploadBlackList(bulkUploadBlackList)
    {
        qRegisterMetaType<PropagatorJob::AbortType>("PropagatorJob::AbortType");
    }

    ~OwncloudPropagator() override;

    void start(SyncFileItemVector &&_syncedItems);

    void startDirectoryPropagation(const SyncFileItemPtr &item,
                                   QStack<QPair<QString, PropagateDirectory*>> &directories,
                                   QVector<PropagatorJob *> &directoriesToRemove,
                                   QString &removedDirectory,
                                   const SyncFileItemVector &items);

    void startFilePropagation(const SyncFileItemPtr &item,
                              QStack<QPair<QString, PropagateDirectory*>> &directories,
                              QVector<PropagatorJob *> &directoriesToRemove,
                              QString &removedDirectory,
                              QString &maybeConflictDirectory);

    void addBulkPropagateDownloadItem(const SyncFileItemPtr &item, QStack<QPair<QString, PropagateDirectory *>> &directories);

    void processE2eeMetadataMigration(const SyncFileItemPtr &item, QStack<QPair<QString, PropagateDirectory *>> &directories);

    [[nodiscard]] const SyncOptions &syncOptions() const;
    void setSyncOptions(const SyncOptions &syncOptions);

    int _downloadLimit = 0;
    int _uploadLimit = 0;
    BandwidthManager _bandwidthManager;

    bool _abortRequested = false;

    /** The list of currently active jobs.
        This list contains the jobs that are currently using resources and is used purely to
        know how many jobs there is currently running for the scheduler.
        Jobs add themself to the list when they do an asynchronous operation.
        Jobs can be several time on the list (example, when several chunks are uploaded in parallel)
     */
    QList<PropagateItemJob *> _activeJobList;

    /** We detected that another sync is required after this one */
    bool _anotherSyncNeeded = false;

    /** Per-folder quota guesses.
     *
     * This starts out empty. When an upload in a folder fails due to insufficient
     * remote quota, the quota guess is updated to be attempted_size-1 at maximum.
     *
     * Note that it will usually just an upper limit for the actual quota - but
     * since the quota on the server might change at any time it can sometimes be
     * wrong in the other direction as well.
     *
     * This allows skipping of uploads that have a very high likelihood of failure.
     */
    QHash<QString, qint64> _folderQuota;

    /* the maximum number of jobs using bandwidth (uploads or downloads, in parallel) */
    int maximumActiveTransferJob();

    /** The size to use for upload chunks.
     *
     * Will be dynamically adjusted after each chunk upload finishes
     * if Capabilities::desiredChunkUploadDuration has a target
     * chunk-upload duration set.
     */
    qint64 _chunkSize;
    qint64 smallFileSize();

    /* The maximum number of active jobs in parallel  */
    int hardMaximumActiveJob();

    /** Check whether a download would clash with an existing file
     * in filesystems that are only case-preserving.
     */
    bool localFileNameClash(const QString &relfile);

    /** Check whether a file is properly accessible for upload.
     *
     * It is possible to create files with filenames that differ
     * only by case in NTFS, but most operations such as stat and
     * open only target one of these by default.
     *
     * When that happens, we want to avoid uploading incorrect data
     * and give up on the file.
     */
    bool hasCaseClashAccessibilityProblem(const QString &relfile);

    [[nodiscard]] QString fullLocalPath(const QString &tmp_file_name) const;
    [[nodiscard]] QString localPath() const;

    /**
     * Returns the full remote path including the folder root of a
     * folder sync path.
     */
    [[nodiscard]] QString fullRemotePath(const QString &tmp_file_name) const;
    [[nodiscard]] QString remotePath() const;

    [[nodiscard]] QString fulllRemotePathToPathInSyncJournalDb(const QString &fullRemotePath) const;

    /** Creates the job for an item.
     */
    PropagateItemJob *createJob(const SyncFileItemPtr &item);

    void scheduleNextJob();
    void reportProgress(const SyncFileItem &, qint64 bytes);

    void abort()
    {
        if (_abortRequested)
            return;

        _abortRequested = true;
        if (_rootJob) {
            // Connect to abortFinished  which signals that abort has been asynchronously finished
            connect(_rootJob.data(), &PropagateDirectory::abortFinished, this, &OwncloudPropagator::emitFinished);

            // Use Queued Connection because we're possibly already in an item's finished stack
            QMetaObject::invokeMethod(_rootJob.data(), "abort", Qt::QueuedConnection,
                                      Q_ARG(PropagatorJob::AbortType, PropagatorJob::AbortType::Asynchronous));

            // Give asynchronous abort 5000 msec to finish on its own
            QTimer::singleShot(5000, this, &OwncloudPropagator::abortTimeout);
        } else {
            // No root job, call emitFinished
            emitFinished(SyncFileItem::NormalError);
        }
    }

    [[nodiscard]] AccountPtr account() const;

    enum DiskSpaceResult {
        DiskSpaceOk,
        DiskSpaceFailure,
        DiskSpaceCritical
    };

    /** Checks whether there's enough disk space available to complete
     *  all jobs that are currently running.
     */
    [[nodiscard]] DiskSpaceResult diskSpaceCheck() const;

    /** Handles a conflict by renaming the file 'item'.
     *
     * Sets up conflict records.
     *
     * It also creates a new upload job in composite if the item that's
     * moved away is a file and conflict uploads are requested.
     *
     * Returns true on success, false and error on error.
     */
    bool createConflict(const SyncFileItemPtr &item,
        PropagatorCompositeJob *composite, QString *error);

    /** Handles a case clash conflict by renaming the file 'item'.
     *
     * Sets up conflict records.
     *
     * Returns true on success, false and error on error.
     */
    OCC::Optional<QString> createCaseClashConflict(const SyncFileItemPtr &item, const QString &temporaryDownloadedFile);

    // Map original path (as in the DB) to target final path
    QMap<QString, QString> _renamedDirectories;
    [[nodiscard]] QString adjustRenamedPath(const QString &original) const;

    /** Update the database for an item.
     *
     * Typically after a sync operation succeeded. Updates the inode from
     * the filesystem.
     *
     * Will also trigger a Vfs::convertToPlaceholder.
     */
    Result<Vfs::ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &item, Vfs::UpdateMetadataTypes updateType = Vfs::AllMetadata);

    /** Update the database for an item.
     *
     * Typically after a sync operation succeeded. Updates the inode from
     * the filesystem.
     *
     * Will also trigger a Vfs::convertToPlaceholder.
     */
    static Result<Vfs::ConvertToPlaceholderResult, QString> staticUpdateMetadata(const SyncFileItem &item,
                                                                                 const QString localDir,
                                                                                 Vfs *vfs,
                                                                                 SyncJournalDb * const journal,
                                                                                 Vfs::UpdateMetadataTypes updateType);

    [[nodiscard]] bool isDelayedUploadItem(const SyncFileItemPtr &item) const;

    [[nodiscard]] const std::deque<SyncFileItemPtr>& delayedTasks() const
    {
        return _delayedTasks;
    }

    void setScheduleDelayedTasks(bool active);

    void clearDelayedTasks();

    void addToBulkUploadBlackList(const QString &file);

    void removeFromBulkUploadBlackList(const QString &file);

    [[nodiscard]] bool isInBulkUploadBlackList(const QString &file) const;

private slots:

    void abortTimeout()
    {
        // Abort synchronously and finish
        _rootJob.data()->abort(PropagatorJob::AbortType::Synchronous);
        emitFinished(SyncFileItem::NormalError);
    }

    /** Emit the finished signal and make sure it is only emitted once */
    void emitFinished(OCC::SyncFileItem::Status status)
    {
        if (!_finishedEmited) {
            emit finished(status);
        }
        _abortRequested = false;
        _finishedEmited = true;
    }

    void scheduleNextJobImpl();

signals:
    void newItem(const OCC::SyncFileItemPtr &);
    void itemCompleted(const OCC::SyncFileItemPtr &item, OCC::ErrorCategory category);
    void progress(const OCC::SyncFileItem &, qint64 bytes);
    void finished(OCC::SyncFileItem::Status status);

    /** Emitted when propagation has problems with a locked file. */
    void seenLockedFile(const QString &fileName);

    /** Emitted when propagation touches a file.
     *
     * Used to track our own file modifications such that notifications
     * from the file watcher about these can be ignored.
     */
    void touchedFile(const QString &fileName);

    void insufficientLocalStorage();
    void insufficientRemoteStorage();

private:
    std::unique_ptr<PropagateUploadFileCommon> createUploadJob(SyncFileItemPtr item,
                                                               bool deleteExisting);

    void pushDelayedUploadTask(SyncFileItemPtr item);

    void resetDelayedUploadTasks();

    static void adjustDeletedFoldersWithNewChildren(SyncFileItemVector &items);

    AccountPtr _account;
    QScopedPointer<PropagateRootDirectory> _rootJob;
    SyncOptions _syncOptions;
    bool _jobScheduled = false;

    const QString _localDir; // absolute path to the local directory. ends with '/'
    const QString _remoteFolder; // remote folder, ends with '/'

    std::deque<SyncFileItemPtr> _delayedTasks;
    bool _scheduleDelayedTasks = false;

    QSet<QString> &_bulkUploadBlackList;

    static bool _allowDelayedUpload;
};


/**
 * @brief Job that wait for all the poll jobs to be completed
 * @ingroup libsync
 */
class CleanupPollsJob : public QObject
{
    Q_OBJECT
    QVector<SyncJournalDb::PollInfo> _pollInfos;
    AccountPtr _account;
    SyncJournalDb *_journal;
    QString _localPath;
    QSharedPointer<Vfs> _vfs;

public:
    explicit CleanupPollsJob(AccountPtr account,
                             SyncJournalDb *journal,
                             const QString &localPath,
                             const QSharedPointer<Vfs> &vfs,
                             QObject *parent = nullptr)
        : QObject(parent)
        , _pollInfos(journal->getPollInfos())
        , _account(account)
        , _journal(journal)
        , _localPath(localPath)
        , _vfs(vfs)
    {
    }

    ~CleanupPollsJob() override;

    /**
     * Start the job.  After the job is completed, it will emit either finished or aborted, and it
     * will destroy itself.
     */
    void start();
signals:
    void finished();
    void aborted(const QString &error, const OCC::ErrorCategory errorCategory);
private slots:
    void slotPollFinished();
};
}

#endif
