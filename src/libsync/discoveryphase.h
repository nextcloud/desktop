/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include <QObject>
#include <QElapsedTimer>
#include <QStringList>
#include <csync.h>
#include <QMap>
#include "networkjobs.h"
#include <QMutex>
#include <QWaitCondition>
#include <QLinkedList>
#include <deque>

namespace OCC {

class Account;

/**
 * The Discovery Phase was once called "update" phase in csync terms.
 * Its goal is to look at the files in one of the remote and check compared to the db
 * if the files are new, or changed.
 */

struct SyncOptions
{
    SyncOptions()
        : _newBigFolderSizeLimit(-1)
        , _confirmExternalStorage(false)
        , _initialChunkSize(10 * 1000 * 1000) // 10 MB
        , _minChunkSize(1 * 1000 * 1000) // 1 MB
        , _maxChunkSize(100 * 1000 * 1000) // 100 MB
        , _targetChunkUploadDuration(60 * 1000) // 1 minute
    {
    }

    /** Maximum size (in Bytes) a folder can have without asking for confirmation.
     * -1 means infinite */
    qint64 _newBigFolderSizeLimit;

    /** If a confirmation should be asked for external storages */
    bool _confirmExternalStorage;

    /** The initial un-adjusted chunk size in bytes for chunked uploads, both
     * for old and new chunking algorithm, which classifies the item to be chunked
     *
     * In chunkingNG, when dynamic chunk size adjustments are done, this is the
     * starting value and is then gradually adjusted within the
     * minChunkSize / maxChunkSize bounds.
     */
    quint64 _initialChunkSize;

    /** The minimum chunk size in bytes for chunked uploads */
    quint64 _minChunkSize;

    /** The maximum chunk size in bytes for chunked uploads */
    quint64 _maxChunkSize;

    /** The target duration of chunk uploads for dynamic chunk sizing.
     *
     * Set to 0 it will disable dynamic chunk sizing.
     */
    quint64 _targetChunkUploadDuration;
};


struct DiscoveryDirectoryResult
{
    QString path;
    QString msg;
    int code;
    std::deque<std::unique_ptr<csync_file_stat_t>> list;
    DiscoveryDirectoryResult()
        : code(EIO)
    {
    }
};

/**
 * @brief The DiscoverySingleDirectoryJob class
 *
 * Run in the main thread, reporting to the DiscoveryJobMainThread object
 *
 * @ingroup libsync
 */
class DiscoverySingleDirectoryJob : public QObject
{
    Q_OBJECT
public:
    explicit DiscoverySingleDirectoryJob(const AccountPtr &account, const QString &path, QObject *parent = 0);
    // Specify thgat this is the root and we need to check the data-fingerprint
    void setIsRootPath() { _isRootPath = true; }
    void start();
    void abort();
    std::deque<std::unique_ptr<csync_file_stat_t>> &&takeResults() { return std::move(_results); }

    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(const QString &);
    void etagConcatenation(const QString &);
    void etag(const QString &);
    void finishedWithResult();
    void finishedWithError(int csyncErrnoCode, const QString &msg);
private slots:
    void directoryListingIteratedSlot(QString, const QMap<QString, QString> &);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply *);

private:
    std::deque<std::unique_ptr<csync_file_stat_t>> _results;
    QString _subPath;
    QString _etagConcatenation;
    QString _firstEtag;
    AccountPtr _account;
    // The first result is for the directory itself and need to be ignored.
    // This flag is true if it was already ignored.
    bool _ignoredFirst;
    // Set to true if this is the root path and we need to check the data-fingerprint
    bool _isRootPath;
    // If this directory is an external storage (The first item has 'M' in its permission)
    bool _isExternalStorage;
    QPointer<LsColJob> _lsColJob;

public:
    QByteArray _dataFingerprint;
};

// Lives in main thread. Deleted by the SyncEngine
class DiscoveryJob;
class DiscoveryMainThread : public QObject
{
    Q_OBJECT

    QPointer<DiscoveryJob> _discoveryJob;
    QPointer<DiscoverySingleDirectoryJob> _singleDirJob;
    QString _pathPrefix; // remote path
    AccountPtr _account;
    DiscoveryDirectoryResult *_currentDiscoveryDirectoryResult;
    qint64 *_currentGetSizeResult;
    bool _firstFolderProcessed;

public:
    DiscoveryMainThread(AccountPtr account)
        : QObject()
        , _account(account)
        , _currentDiscoveryDirectoryResult(0)
        , _currentGetSizeResult(0)
        , _firstFolderProcessed(false)
    {
    }
    void abort();

    QByteArray _dataFingerprint;


public slots:
    // From DiscoveryJob:
    void doOpendirSlot(const QString &url, DiscoveryDirectoryResult *);
    void doGetSizeSlot(const QString &path, qint64 *result);

    // From Job:
    void singleDirectoryJobResultSlot();
    void singleDirectoryJobFinishedWithErrorSlot(int csyncErrnoCode, const QString &msg);
    void singleDirectoryJobFirstDirectoryPermissionsSlot(const QString &);

    void slotGetSizeFinishedWithError();
    void slotGetSizeResult(const QVariantMap &);
signals:
    void etag(const QString &);
    void etagConcatenation(const QString &);

public:
    void setupHooks(DiscoveryJob *discoveryJob, const QString &pathPrefix);
};

/**
 * @brief The DiscoveryJob class
 *
 * Lives in the other thread, deletes itself in !start()
 *
 * @ingroup libsync
 */
class DiscoveryJob : public QObject
{
    Q_OBJECT
    friend class DiscoveryMainThread;
    CSYNC *_csync_ctx;
    csync_log_callback _log_callback;
    int _log_level;
    QElapsedTimer _lastUpdateProgressCallbackCall;

    /**
     * return true if the given path should be ignored,
     * false if the path should be synced
     */
    bool isInSelectiveSyncBlackList(const char *path) const;
    static int isInSelectiveSyncBlackListCallback(void *, const char *);
    bool checkSelectiveSyncNewFolder(const QString &path, const char *remotePerm);
    static int checkSelectiveSyncNewFolderCallback(void *data, const char *path, const char *remotePerm);

    // Just for progress
    static void update_job_update_callback(bool local,
        const char *dirname,
        void *userdata);

    // For using QNAM to get the directory listings
    static csync_vio_handle_t *remote_vio_opendir_hook(const char *url,
        void *userdata);
    static std::unique_ptr<csync_file_stat_t> remote_vio_readdir_hook(csync_vio_handle_t *dhandle,
        void *userdata);
    static void remote_vio_closedir_hook(csync_vio_handle_t *dhandle,
        void *userdata);
    QMutex _vioMutex;
    QWaitCondition _vioWaitCondition;


public:
    explicit DiscoveryJob(CSYNC *ctx, QObject *parent = 0)
        : QObject(parent)
        , _csync_ctx(ctx)
    {
        // We need to forward the log property as csync uses thread local
        // and updates run in another thread
        _log_callback = csync_get_log_callback();
        _log_level = csync_get_log_level();
    }

    QStringList _selectiveSyncBlackList;
    QStringList _selectiveSyncWhiteList;
    SyncOptions _syncOptions;
    Q_INVOKABLE void start();
signals:
    void finished(int result);
    void folderDiscovered(bool local, QString folderUrl);

    // After the discovery job has been woken up again (_vioWaitCondition)
    void doOpendirSignal(QString url, DiscoveryDirectoryResult *);
    void doGetSizeSignal(const QString &path, qint64 *result);

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);
};
}
