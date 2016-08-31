/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#ifndef OWNCLOUDPROPAGATOR_H
#define OWNCLOUDPROPAGATOR_H

#include <QHash>
#include <QObject>
#include <QMap>
#include <QLinkedList>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QIODevice>
#include <QMutex>

#include "syncfileitem.h"
#include "syncjournaldb.h"
#include "bandwidthmanager.h"
#include "accountfwd.h"

namespace OCC {

/** Free disk space threshold below which syncs will abort and not even start.
 */
qint64 criticalFreeSpaceLimit();

/** The client will not intentionally reduce the available free disk space below
 *  this limit.
 *
 * Uploads will still run and downloads that are small enough will continue too.
 */
qint64 freeSpaceLimit();

class SyncJournalDb;
class OwncloudPropagator;

/**
 * @brief the base class of propagator jobs
 *
 * This can either be a job, or a container for jobs.
 * If it is a composite job, it then inherits from PropagateDirectory
 *
 * @ingroup libsync
 */
class PropagatorJob : public QObject {
    Q_OBJECT
protected:
    OwncloudPropagator *_propagator;

public:
    explicit PropagatorJob(OwncloudPropagator* propagator) : _propagator(propagator), _state(NotYetStarted) {}

    enum JobState {
        NotYetStarted,
        Running,
        Finished
    };
    JobState _state;

    enum JobParallelism {

        /** Jobs can be run in parallel to this job */
        FullParallelism,

        /** No other job shall be started until this one has finished.
            So this job is guaranteed to finish before any jobs below it
            are executed. */
        WaitForFinished,

        /** A job with this parallelism will allow later jobs to start and
            run in parallel as long as they aren't PropagateDirectory jobs.
            When the first directory job is encountered, no further jobs
            will be started until this one is finished. */
        WaitForFinishedInParentDirectory
    };

    virtual JobParallelism parallelism() { return FullParallelism; }

    /**
     * For "small" jobs
     */
    virtual bool isLikelyFinishedQuickly() { return false; }

    /** The space that the running jobs need to complete but don't actually use yet.
     *
     * Note that this does *not* include the disk space that's already
     * in use by running jobs for things like a download-in-progress.
     */
    virtual qint64 committedDiskSpace() const { return 0; }

public slots:
    virtual void abort() {}

    /** Starts this job, or a new subjob
     * returns true if a job was started.
     */
    virtual bool scheduleNextJob() = 0;
signals:
    /**
     * Emitted when the job is fully finished
     */
    void finished(SyncFileItem::Status);

    /**
     * Emitted when one item has been completed within a job.
     */
    void itemCompleted(const SyncFileItem &, const PropagatorJob &);

    /**
     * Emitted when all the sub-jobs have been finished and
     * more jobs might be started  (so scheduleNextJob can/must be called again)
     */
    void ready();

    void progress(const SyncFileItem& item, quint64 bytes);

};


/*
 * Abstract class to propagate a single item
 */
class PropagateItemJob : public PropagatorJob {
    Q_OBJECT
protected:
    void done(SyncFileItem::Status status, const QString &errorString = QString());

    bool checkForProblemsWithShared(int httpStatusCode, const QString& msg);

    /*
     * set a custom restore job message that is used if the restore job succeeded.
     * It is displayed in the activity view.
     */
    QString restoreJobMsg() const {
        return _item->_isRestoration ? _item->_errorString : QString();
    }
    void setRestoreJobMsg( const QString& msg = QString() ) {
        _item->_isRestoration = true;
        _item->_errorString = msg;
    }

protected slots:
    void slotRestoreJobCompleted(const SyncFileItem& );

private:
    QScopedPointer<PropagateItemJob> _restoreJob;

public:
    PropagateItemJob(OwncloudPropagator* propagator, const SyncFileItemPtr &item)
        : PropagatorJob(propagator), _item(item) {}

    bool scheduleNextJob() Q_DECL_OVERRIDE {
        if (_state != NotYetStarted) {
            return false;
        }
        _state = Running;
        QMetaObject::invokeMethod(this, "start"); // We could be in a different thread (neon jobs)
        return true;
    }

    SyncFileItemPtr  _item;

public slots:
    virtual void start() = 0;
};


/**
 * @brief Propagate a directory, and all its sub entries.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT PropagateDirectory : public PropagatorJob {
    Q_OBJECT
public:
    // e.g: create the directory
    QScopedPointer<PropagateItemJob>_firstJob;

    // all the sub files or sub directories.
    QVector<PropagatorJob *> _subJobs;

    SyncFileItemPtr _item;

    int _jobsFinished; // number of jobs that have completed
    int _runningNow; // number of subJobs running right now
    SyncFileItem::Status _hasError;  // NoStatus,  or NormalError / SoftError if there was an error

    explicit PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItemPtr &item = SyncFileItemPtr(new SyncFileItem))
        : PropagatorJob(propagator)
        , _firstJob(0), _item(item),  _jobsFinished(0), _runningNow(0), _hasError(SyncFileItem::NoStatus)
    { }

    virtual ~PropagateDirectory() {
        qDeleteAll(_subJobs);
    }

    void append(PropagatorJob *subJob) {
        _subJobs.append(subJob);
    }

    virtual bool scheduleNextJob() Q_DECL_OVERRIDE;
    virtual JobParallelism parallelism() Q_DECL_OVERRIDE;
    virtual void abort() Q_DECL_OVERRIDE {
        if (_firstJob)
            _firstJob->abort();
        foreach (PropagatorJob *j, _subJobs)
            j->abort();
    }

    void increaseAffectedCount() {
        _firstJob->_item->_affectedItems++;
    }

    void finalize();

    qint64 committedDiskSpace() const Q_DECL_OVERRIDE;

private slots:
    bool possiblyRunNextJob(PropagatorJob *next) {
        if (next->_state == NotYetStarted) {
            connect(next, SIGNAL(finished(SyncFileItem::Status)), this, SLOT(slotSubJobFinished(SyncFileItem::Status)), Qt::QueuedConnection);
            connect(next, SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)),
                    this, SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
            connect(next, SIGNAL(progress(const SyncFileItem &,quint64)), this, SIGNAL(progress(const SyncFileItem &,quint64)));
            connect(next, SIGNAL(ready()), this, SIGNAL(ready()));
            _runningNow++;
        }
        return next->scheduleNextJob();
    }

    void slotSubJobFinished(SyncFileItem::Status status);
};


/**
 * @brief Dummy job that just mark it as completed and ignored
 * @ingroup libsync
 */
class PropagateIgnoreJob : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateIgnoreJob(OwncloudPropagator* propagator,const SyncFileItemPtr& item)
        : PropagateItemJob(propagator, item) {}
    void start() Q_DECL_OVERRIDE {
        SyncFileItem::Status status = _item->_status;
        done(status == SyncFileItem::NoStatus ? SyncFileItem::FileIgnored : status, _item->_errorString);
    }
};

class OwncloudPropagator : public QObject {
    Q_OBJECT

    PropagateItemJob *createJob(const SyncFileItemPtr& item);
    QScopedPointer<PropagateDirectory> _rootJob;

public:
    const QString _localDir; // absolute path to the local directory. ends with '/'
    const QString _remoteDir; // path to the root of the remote. ends with '/'  (include WebDAV path)
    const QString _remoteFolder; // folder. (same as remoteDir but without the WebDAV path)

    SyncJournalDb * const _journal;
    bool _finishedEmited; // used to ensure that finished is only emitted once


public:
    OwncloudPropagator(AccountPtr account, const QString &localDir,
                       const QString &remoteDir, const QString &remoteFolder,
                       SyncJournalDb *progressDb)
            : _localDir((localDir.endsWith(QChar('/'))) ? localDir : localDir+'/' )
            , _remoteDir((remoteDir.endsWith(QChar('/'))) ? remoteDir : remoteDir+'/' )
            , _remoteFolder((remoteFolder.endsWith(QChar('/'))) ? remoteFolder : remoteFolder+'/' )
            , _journal(progressDb)
            , _finishedEmited(false)
            , _bandwidthManager(this)
            , _anotherSyncNeeded(false)
            , _account(account)
    { }

    ~OwncloudPropagator();

    void start(const SyncFileItemVector &_syncedItems);

    QAtomicInt _downloadLimit;
    QAtomicInt _uploadLimit;
    BandwidthManager _bandwidthManager;

    QAtomicInt _abortRequested; // boolean set by the main thread to abort.

    /** The list of currently active jobs.
        This list contains the jobs that are currently using ressources and is used purely to
        know how many jobs there is currently running for the scheduler.
        Jobs add themself to the list when they do an assynchronous operation.
        Jobs can be several time on the list (example, when several chunks are uploaded in parallel)
     */
    QList<PropagateItemJob*> _activeJobList;

    /** We detected that another sync is required after this one */
    bool _anotherSyncNeeded;

    /* The maximum number of active jobs in parallel  */
    int maximumActiveJob();
    int hardMaximumActiveJob();

    bool isInSharedDirectory(const QString& file);
    bool localFileNameClash(const QString& relfile);
    QString getFilePath(const QString& tmp_file_name) const;

    void abort() {
        _abortRequested.fetchAndStoreOrdered(true);
        if (_rootJob) {
            _rootJob->abort();
        }
        emitFinished(SyncFileItem::NormalError);
    }

    // timeout in seconds
    static int httpTimeout();

    /** returns the size of chunks in bytes  */
    static quint64 chunkSize();

    AccountPtr account() const;

    enum DiskSpaceResult
    {
        DiskSpaceOk,
        DiskSpaceFailure,
        DiskSpaceCritical
    };

    /** Checks whether there's enough disk space available to complete
     *  all jobs that are currently running.
     */
    DiskSpaceResult diskSpaceCheck() const;



private slots:

    /** Emit the finished signal and make sure it is only emitted once */
    void emitFinished(SyncFileItem::Status status) {
        if (!_finishedEmited)
            emit finished(status == SyncFileItem::Success);
        _finishedEmited = true;
    }

    void scheduleNextJob();

signals:
    void itemCompleted(const SyncFileItem &, const PropagatorJob &);
    void progress(const SyncFileItem&, quint64 bytes);
    void finished(bool success);

    /** Emitted when propagation has problems with a locked file. */
    void seenLockedFile(const QString &fileName);

    /** Emitted when propagation touches a file.
     *
     * Used to track our own file modifications such that notifications
     * from the file watcher about these can be ignored.
     */
    void touchedFile(const QString &fileName);

private:

    AccountPtr _account;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    // access to signals which are protected in Qt4
    friend class PropagateDownloadFile;
    friend class PropagateUploadFile;
    friend class PropagateLocalMkdir;
    friend class PropagateLocalRename;
    friend class PropagateRemoteMove;
#endif
};


/**
 * @brief Job that wait for all the poll jobs to be completed
 * @ingroup libsync
 */
class CleanupPollsJob : public QObject {
    Q_OBJECT
    QVector< SyncJournalDb::PollInfo > _pollInfos;
    AccountPtr _account;
    SyncJournalDb *_journal;
    QString _localPath;
public:
    explicit CleanupPollsJob(const QVector< SyncJournalDb::PollInfo > &pollInfos, AccountPtr account,
                             SyncJournalDb *journal, const QString &localPath, QObject* parent = 0)
        : QObject(parent), _pollInfos(pollInfos), _account(account), _journal(journal), _localPath(localPath) {}

    ~CleanupPollsJob();

    /**
     * Start the job.  After the job is completed, it will emit either finished or aborted, and it
     * will destroy itself.
     */
    void start();
signals:
    void finished();
    void aborted(const QString &error);
private slots:
    void slotPollFinished();
};

}

#endif
