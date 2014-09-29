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

#include <neon/ne_request.h>
#include <QHash>
#include <QObject>
#include <QMap>
#include <QLinkedList>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QIODevice>

#include "syncfileitem.h"
#include "syncjournaldb.h"
#include "bandwidthmanager.h"

struct hbf_transfer_s;
struct ne_session_s;
struct ne_decompress_s;
typedef struct ne_prop_result_set_s ne_prop_result_set;

namespace Mirall {

class Account;

class SyncJournalDb;
class OwncloudPropagator;

class PropagatorJob : public QObject {
    Q_OBJECT
protected:
    OwncloudPropagator *_propagator;
    void emitReady() {
        bool wasReady = _readySent;
        _readySent = true;
        if (!wasReady)
            emit ready();
    };
public:
    bool _readySent;
    explicit PropagatorJob(OwncloudPropagator* propagator) : _propagator(propagator), _readySent(false) {}

public slots:
    virtual void start() = 0;
    virtual void abort() {}
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
     * Emitted when all the sub-jobs have been scheduled and
     * we are ready and more jobs might be started
     * This signal is not always emitted.
     */
    void ready();

    void progress(const SyncFileItem& item, quint64 bytes);

};

/*
 * Propagate a directory, and all its sub entries.
 */
class PropagateDirectory : public PropagatorJob {
    Q_OBJECT
public:
    // e.g: create the directory
    QScopedPointer<PropagatorJob>_firstJob;

    // all the sub files or sub directories.
    QVector<PropagatorJob *> _subJobs;

    SyncFileItem _item;

    int _current; // index of the current running job
    int _runningNow; // number of subJob running now
    SyncFileItem::Status _hasError;  // NoStatus,  or NormalError / SoftError if there was an error


    explicit PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItem &item = SyncFileItem())
        : PropagatorJob(propagator)
        , _firstJob(0), _item(item),  _current(-1), _runningNow(0), _hasError(SyncFileItem::NoStatus) { }

    virtual ~PropagateDirectory() {
        qDeleteAll(_subJobs);
    }

    void append(PropagatorJob *subJob) {
        _subJobs.append(subJob);
    }

    virtual void start();
    virtual void abort() {
        if (_firstJob)
            _firstJob->abort();
        foreach (PropagatorJob *j, _subJobs)
            j->abort();
    }

private slots:
    void startJob(PropagatorJob *next) {
        connect(next, SIGNAL(finished(SyncFileItem::Status)), this, SLOT(slotSubJobFinished(SyncFileItem::Status)), Qt::QueuedConnection);
        connect(next, SIGNAL(completed(SyncFileItem)), this, SIGNAL(completed(SyncFileItem)));
        connect(next, SIGNAL(progress(SyncFileItem,quint64)), this, SIGNAL(progress(SyncFileItem,quint64)));
        connect(next, SIGNAL(ready()), this, SLOT(slotSubJobReady()));
        _runningNow++;
        QMetaObject::invokeMethod(next, "start", Qt::QueuedConnection);
    }

    void slotSubJobFinished(SyncFileItem::Status status);
    void slotSubJobReady();
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
        return _item._isRestoration ? _item._errorString : QString();
    }
    void setRestoreJobMsg( const QString& msg = QString() ) {
        _item._isRestoration = true;
        _item._errorString = msg;
    }

    SyncFileItem  _item;

protected slots:
    void slotRestoreJobCompleted(const SyncFileItem& );

private:
    QScopedPointer<PropagateItemJob> _restoreJob;

public:
    PropagateItemJob(OwncloudPropagator* propagator, const SyncFileItem &item)
        : PropagatorJob(propagator), _item(item) {}

};

// Dummy job that just mark it as completed and ignored.
class PropagateIgnoreJob : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateIgnoreJob(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item) {}
    void start() {
        SyncFileItem::Status status = _item._status;
        done(status == SyncFileItem::NoStatus ? SyncFileItem::FileIgnored : status, _item._errorString);
    }
};

class BandwidthManager; // fwd
class UploadDevice : public QIODevice {
    Q_OBJECT
public:
    QPointer<QIODevice> _file;
    qint64 _read;
    qint64 _size;
    qint64 _start;
    BandwidthManager* _bandwidthManager;

    qint64 _bandwidthQuota;
    qint64 _readWithProgress;

    UploadDevice(QIODevice *file,  qint64 start, qint64 size, BandwidthManager *bwm);
    ~UploadDevice();
    virtual qint64 writeData(const char* , qint64 );
    virtual qint64 readData(char* data, qint64 maxlen);
    virtual bool atEnd() const;
    virtual qint64 size() const;
    qint64 bytesAvailable() const;
    virtual bool isSequential() const;
    virtual bool seek ( qint64 pos );

    void setBandwidthLimited(bool);
    bool isBandwidthLimited() { return _bandwidthLimited; }
    void setChoked(bool);
    bool isChoked() { return _choked; }
    void giveBandwidthQuota(qint64 bwq);
private:
    bool _bandwidthLimited; // if _bandwidthQuota will be used
    bool _choked; // if upload is paused (readData() will return 0)
protected slots:
    void slotJobUploadProgress(qint64 sent, qint64 t);
};
//Q_DECLARE_METATYPE(UploadDevice);
//Q_DECLARE_METATYPE(QPointer<UploadDevice>);


class OwncloudPropagator : public QObject {
    Q_OBJECT

    PropagateItemJob *createJob(const SyncFileItem& item);
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

    BandwidthManager _bandwidthManager;

public:
    OwncloudPropagator(ne_session_s *session, const QString &localDir, const QString &remoteDir, const QString &remoteFolder,
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
    { }

    void start(const SyncFileItemVector &_syncedItems);

    QAtomicInt _downloadLimit;
    QAtomicInt _uploadLimit;

    QAtomicInt _abortRequested; // boolean set by the main thread to abort.

    /* The number of currently active jobs */
    int _activeJobs;

    /** We detected that another sync is required after this one */
    bool _anotherSyncNeeded;

    /* The maximum number of active job in parallel  */
    int maximumActiveJob();

    bool isInSharedDirectory(const QString& file);
    bool localFileNameClash(const QString& relfile);

    void abort() {
        _abortRequested.fetchAndStoreOrdered(true);
        if (_rootJob) {
            _rootJob->abort();
        }
        emitFinished();
    }

    // timeout in seconds
    static int httpTimeout();

private slots:

    /** Emit the finished signal and make sure it is only emit once */
    void emitFinished() {
        if (!_finishedEmited)
            emit finished();
        _finishedEmited = true;
    }

signals:
    void completed(const SyncFileItem &);
    void progress(const SyncFileItem&, quint64 bytes);
    void finished();
    /**
     * Called when we detect that the total number of bytes changes (because a download or upload
     * turns out to be bigger or smaller than what was initially computed in the update phase
     */
    void adjustTotalTransmissionSize( qint64 adjust );

};

// Job that wait for all the poll jobs to be completed
class CleanupPollsJob : public QObject {
    Q_OBJECT
    QVector< SyncJournalDb::PollInfo > _pollInfos;
    Account *_account;
    SyncJournalDb *_journal;
    QString _localPath;
public:
    explicit CleanupPollsJob(const QVector< SyncJournalDb::PollInfo > &pollInfos, Account *account,
                             SyncJournalDb *journal, const QString &localPath, QObject* parent = 0)
        : QObject(parent), _pollInfos(pollInfos), _account(account), _journal(journal), _localPath(localPath) {}

    void start();
signals:
    void finished();
    void aborted(const QString &error);
private slots:
    void slotPollFinished();
};

}

#endif
