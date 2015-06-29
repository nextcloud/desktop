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

namespace OCC {

class Account;

/**
 * The Discovery Phase was once called "update" phase in csync therms.
 * Its goal is to look at the files in one of the remote and check comared to the db
 * if the files are new, or changed.
 */

/**
 * @brief The FileStatPointer class
 * @ingroup libsync
 */
class FileStatPointer {
public:
    FileStatPointer(csync_vio_file_stat_t *stat)
        : _stat(stat)
    { }
    FileStatPointer(const FileStatPointer &other)
        : _stat(csync_vio_file_stat_copy(other._stat))
    { }
    ~FileStatPointer() {
        csync_vio_file_stat_destroy(_stat);
    }
    FileStatPointer &operator=(const FileStatPointer &other) {
        csync_vio_file_stat_destroy(_stat);
        _stat = csync_vio_file_stat_copy(other._stat);
        return *this;
    }
    inline csync_vio_file_stat_t *data() const { return _stat; }
    inline csync_vio_file_stat_t *operator->() const { return _stat; }

private:
    csync_vio_file_stat_t *_stat;
};

struct DiscoveryDirectoryResult {
    QString path;
    QString msg;
    int code;
    QList<FileStatPointer> list;
    int listIndex;
    DiscoveryDirectoryResult() : code(EIO), listIndex(0) { }
};

/**
 * @brief The DiscoverySingleDirectoryJob class
 *
 * Run in the main thread, reporting to the DiscoveryJobMainThread object
 *
 * @ingroup libsync
 */
class DiscoverySingleDirectoryJob : public QObject {
    Q_OBJECT
public:
    explicit DiscoverySingleDirectoryJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start();
    void abort();
    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(const QString &);
    void etagConcatenation(const QString &);
    void finishedWithResult(const QList<FileStatPointer> &);
    void finishedWithError(int csyncErrnoCode, QString msg);
private slots:
    void directoryListingIteratedSlot(QString,QMap<QString,QString>);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply*);
private:
    QList<FileStatPointer> _results;
    QString _subPath;
    QString _etagConcatenation;
    AccountPtr _account;
    bool _ignoredFirst;
    QPointer<LsColJob> _lsColJob;
};

// Lives in main thread. Deleted by the SyncEngine
class DiscoveryJob;
class DiscoveryMainThread : public QObject {
    Q_OBJECT

    QPointer<DiscoveryJob> _discoveryJob;
    QPointer<DiscoverySingleDirectoryJob> _singleDirJob;
    QString _pathPrefix; // remote path
    AccountPtr _account;
    DiscoveryDirectoryResult *_currentDiscoveryDirectoryResult;
    qint64 *_currentGetSizeResult;

public:
    DiscoveryMainThread(AccountPtr account) : QObject(), _account(account),
        _currentDiscoveryDirectoryResult(0), _currentGetSizeResult(0)
    { }
    void abort();


public slots:
    // From DiscoveryJob:
    void doOpendirSlot(QString url, DiscoveryDirectoryResult* );
    void doGetSizeSlot(const QString &path ,qint64 *result);

    // From Job:
    void singleDirectoryJobResultSlot(const QList<FileStatPointer> &);
    void singleDirectoryJobFinishedWithErrorSlot(int csyncErrnoCode, QString msg);
    void singleDirectoryJobFirstDirectoryPermissionsSlot(QString);

    void slotGetSizeFinishedWithError();
    void slotGetSizeResult(const QVariantMap&);
signals:
    void etagConcatenation(QString);
public:
    void setupHooks(DiscoveryJob* discoveryJob, const QString &pathPrefix);
};

/**
 * @brief The DiscoveryJob class
 *
 * Lives in the other thread, deletes itself in !start()
 *
 * @ingroup libsync
 */
class DiscoveryJob : public QObject {
    Q_OBJECT
    friend class DiscoveryMainThread;
    CSYNC              *_csync_ctx;
    csync_log_callback  _log_callback;
    int                 _log_level;
    void*               _log_userdata;
    QElapsedTimer       _lastUpdateProgressCallbackCall;

    /**
     * return true if the given path should be ignored,
     * false if the path should be synced
     */
    bool isInSelectiveSyncBlackList(const QString &path) const;
    static int isInSelectiveSyncBlackListCallback(void *, const char *);
    bool checkSelectiveSyncNewShare(const QString &path);
    static int checkSelectiveSyncNewShareCallback(void*, const char*);

    // Just for progress
    static void update_job_update_callback (bool local,
                                            const char *dirname,
                                            void *userdata);

    // For using QNAM to get the directory listings
    static csync_vio_handle_t* remote_vio_opendir_hook (const char *url,
                                        void *userdata);
    static csync_vio_file_stat_t* remote_vio_readdir_hook (csync_vio_handle_t *dhandle,
                                                                  void *userdata);
    static void remote_vio_closedir_hook (csync_vio_handle_t *dhandle,
                                                                  void *userdata);
    QMutex _vioMutex;
    QWaitCondition _vioWaitCondition;


public:
    explicit DiscoveryJob(CSYNC *ctx, QObject* parent = 0)
            : QObject(parent), _csync_ctx(ctx), _newSharedFolderSizeLimit(-1) {
        // We need to forward the log property as csync uses thread local
        // and updates run in another thread
        _log_callback = csync_get_log_callback();
        _log_level = csync_get_log_level();
        _log_userdata = csync_get_log_userdata();
    }

    QStringList _selectiveSyncBlackList;
    QStringList _selectiveSyncWhiteList;
    qint64 _newSharedFolderSizeLimit;
    Q_INVOKABLE void start();
signals:
    void finished(int result);
    void folderDiscovered(bool local, QString folderUrl);

    // After the discovery job has been woken up again (_vioWaitCondition)
    void doOpendirSignal(QString url, DiscoveryDirectoryResult*);
    void doGetSizeSignal(const QString &path, qint64 *result);

    // A new shared folder was discovered and was not synced because of the confirmation feature
    void newSharedFolder(const QString &folder);
};

}
