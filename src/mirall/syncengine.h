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
#include <qelapsedtimer.h>

#include <csync.h>

#include "mirall/syncfileitem.h"
#include "mirall/progressdispatcher.h"
#include "mirall/utility.h"

class QProcess;

namespace Mirall {

class SyncJournalFileRecord;

class SyncJournalDb;

class OwncloudPropagator;

void OWNCLOUDSYNC_EXPORT csyncLogCatcher(int /*verbosity*/,
                     const char */*function*/,
                     const char *buffer,
                     void */*userdata*/);

class OWNCLOUDSYNC_EXPORT SyncEngine : public QObject
{
    Q_OBJECT
public:
    SyncEngine(CSYNC *, const QString &localPath, const QString &remoteURL, const QString &remotePath, SyncJournalDb *journal);
    ~SyncEngine();

    static QString csyncErrorToString( CSYNC_STATUS);

    Q_INVOKABLE void startSync();
    Q_INVOKABLE void setNetworkLimits();

    /* Abort the sync.  Called from the main thread */
    void abort();

    Utility::StopWatch &stopWatch() { return _stopWatch; }

signals:
    void csyncError( const QString& );
    void csyncUnavailable();

    // before actual syncing (after update+reconcile) for each item
    void syncItemDiscovered(const SyncFileItem&);
    // after the above signals. with the items that actually need propagating
    void aboutToPropagate(const SyncFileItemVector&);

    // after each job (successful or not)
    void jobCompleted(const SyncFileItem&);

    // after sync is done
    void treeWalkResult(const SyncFileItemVector&);

    void transmissionProgress( const Progress::Info& progress );

    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

    void aboutToRemoveAllFiles(SyncFileItem::Direction direction, bool *cancel);

private slots:
    void slotJobCompleted(const SyncFileItem& item);
    void slotFinished();
    void slotProgress(const SyncFileItem& item, quint64 curent);
    void slotAdjustTotalTransmissionSize(qint64 change);
    void slotUpdateFinished(int updateResult);

private:
    void handleSyncError(CSYNC *ctx, const char *state);

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    bool checkBlacklisting( SyncFileItem *item );

    // cleanup and emit the finished signal
    void finalize();

    static QMutex _syncMutex;
    SyncFileItemVector _syncedItems;

    CSYNC *_csync_ctx;
    bool _needsUpdate;
    QString _localPath;
    QString _remoteUrl;
    QString _remotePath;
    SyncJournalDb *_journal;
    QScopedPointer <OwncloudPropagator> _propagator;
    QString _lastDeleted; // if the last item was a path and it has been deleted
    QHash <QString, QString> _seenFiles;
    QThread _thread;

    Progress::Info _progressInfo;

    Utility::StopWatch _stopWatch;

    // maps the origin and the target of the folders that have been renamed
    QHash<QString, QString> _renamedFolders;
    QString adjustRenamedPath(const QString &original);

    bool _hasFiles; // true if there is at least one file that is not ignored or removed

    int _downloadLimit;
    int _uploadLimit;
};


class UpdateJob : public QObject {
    Q_OBJECT
    CSYNC *_csync_ctx;
    csync_log_callback _log_callback;
    int _log_level;
    void* _log_userdata;
    Q_INVOKABLE void start() {
        csync_set_log_callback(_log_callback);
        csync_set_log_level(_log_level);
        csync_set_log_userdata(_log_userdata);
        emit finished(csync_update(_csync_ctx));
        deleteLater();
    }
public:
    explicit UpdateJob(CSYNC *ctx, QObject* parent = 0)
            : QObject(parent), _csync_ctx(ctx) {
        // We need to forward the log property as csync uses thread local
        // and updates run in another thread
        _log_callback = csync_get_log_callback();
        _log_level = csync_get_log_level();
        _log_userdata = csync_get_log_userdata();
    }
signals:
    void finished(int result);
};


}

#endif // CSYNCTHREAD_H
