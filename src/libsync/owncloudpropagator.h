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

struct hbf_transfer_s;
struct ne_session_s;
struct ne_decompress_s;
typedef struct ne_prop_result_set_s ne_prop_result_set;

namespace OCC {

class SyncJournalDb;
class OwncloudPropagator;

/**
 * @brief the base class of propagator jobs
 *
 * This can either be a job, or a container for jobs.
 * If it is a composite jobs, it then inherits from PropagateDirectory
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
        /** This job do not support parallelism, and no other job shall
            be started until this one has finished */
        WaitForFinished,

        /** This job support paralelism with other jobs in the same directory, but it should
             not be paralelized with jobs in other directories  (typically a move operation) */
        WaitForFinishedInParentDirectory
    };

    virtual JobParallelism parallelism() { return FullParallelism; }

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
    void completed(const SyncFileItem &);

    /**
     * Emitted when all the sub-jobs have been finished and
     * more jobs might be started  (so scheduleNextJob can/must be called again)
     */
    void ready();

    void progress(const SyncFileItem& item, quint64 bytes);

};


/*
 * Abstract class to propagate a single item
 * (Only used for neon job)
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
class PropagateDirectory : public PropagatorJob {
    Q_OBJECT
public:
    // e.g: create the directory
    QScopedPointer<PropagateItemJob>_firstJob;

    // all the sub files or sub directories.
    QVector<PropagatorJob *> _subJobs;

    SyncFileItemPtr _item;

    int _current; // index of the current running job
    int _runningNow; // number of subJob running now
    SyncFileItem::Status _hasError;  // NoStatus,  or NormalError / SoftError if there was an error

    explicit PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItemPtr &item = SyncFileItemPtr(new SyncFileItem))
        : PropagatorJob(propagator)
        , _firstJob(0), _item(item),  _current(-1), _runningNow(0), _hasError(SyncFileItem::NoStatus)
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

private slots:
    bool possiblyRunNextJob(PropagatorJob *next) {
        if (next->_state == NotYetStarted) {
            connect(next, SIGNAL(finished(SyncFileItem::Status)), this, SLOT(slotSubJobFinished(SyncFileItem::Status)), Qt::QueuedConnection);
            connect(next, SIGNAL(completed(const SyncFileItem &)), this, SIGNAL(completed(const SyncFileItem &)));
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
    bool useLegacyJobs();

public:
    /* 'const' because they are accessed by the thread */

    QThread* _neonThread;
    ne_session_s * const _session;

    const QString _localDir; // absolute path to the local directory. ends with '/'
    const QString _remoteDir; // path to the root of the remote. ends with '/'  (include remote.php/webdav)
    const QString _remoteFolder; // folder. (same as remoteDir but without remote.php/webdav)

    SyncJournalDb * const _journal;
    bool _finishedEmited; // used to ensure that finished is only emit once


public:
    OwncloudPropagator(AccountPtr account, ne_session_s *session, const QString &localDir,
                       const QString &remoteDir, const QString &remoteFolder,
                       SyncJournalDb *progressDb, QThread *neonThread)
            : _neonThread(neonThread)
            , _session(session)
            , _localDir((localDir.endsWith(QChar('/'))) ? localDir : localDir+'/' )
            , _remoteDir((remoteDir.endsWith(QChar('/'))) ? remoteDir : remoteDir+'/' )
            , _remoteFolder((remoteFolder.endsWith(QChar('/'))) ? remoteFolder : remoteFolder+'/' )
            , _journal(progressDb)
            , _finishedEmited(false)
            , _bandwidthManager(this)
            , _activeJobs(0)
            , _anotherSyncNeeded(false)
            , _account(account)
    { }

    ~OwncloudPropagator();

    void start(const SyncFileItemVector &_syncedItems);

    QAtomicInt _downloadLimit;
    QAtomicInt _uploadLimit;
    BandwidthManager _bandwidthManager;

    QAtomicInt _abortRequested; // boolean set by the main thread to abort.

    /* The number of currently active jobs */
    int _activeJobs;

    /** We detected that another sync is required after this one */
    bool _anotherSyncNeeded;

    /* The maximum number of active job in parallel  */
    int maximumActiveJob();

    bool isInSharedDirectory(const QString& file);
    bool localFileNameClash(const QString& relfile);
    QString getFilePath(const QString& tmp_file_name) const;

    void abort() {
        _abortRequested.fetchAndStoreOrdered(true);
        if (_rootJob) {
            _rootJob->abort();
        }
        emitFinished();
    }

    // timeout in seconds
    static int httpTimeout();

    /** Records that a file was touched by a job.
     *
     * Thread-safe.
     */
    void addTouchedFile(const QString& fn);

    /** Get the ms since a file was touched, or -1 if it wasn't.
     *
     * Thread-safe.
     */
    qint64 timeSinceFileTouched(const QString& fn) const;

    AccountPtr account() const;


private slots:

    /** Emit the finished signal and make sure it is only emit once */
    void emitFinished() {
        if (!_finishedEmited)
            emit finished();
        _finishedEmited = true;
    }

    void scheduleNextJob();

signals:
    void completed(const SyncFileItem &);
    void progress(const SyncFileItem&, quint64 bytes);
    void finished();
    /**
     * Called when we detect that the total number of bytes changes (because a download or upload
     * turns out to be bigger or smaller than what was initially computed in the update phase
     */
    void adjustTotalTransmissionSize( qint64 adjust );

private:

    AccountPtr _account;

    /** Stores the time since a job touched a file. */
    QHash<QString, QElapsedTimer> _touchedFiles;
    mutable QMutex _touchedFilesMutex;
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

    void start();
signals:
    void finished();
    void aborted(const QString &error);
private slots:
    void slotPollFinished();
};

}

#endif
