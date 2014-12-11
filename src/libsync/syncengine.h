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
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QSharedPointer>

#include <csync.h>

// when do we go away with this private/public separation?
#include <csync_private.h>

#include "syncfileitem.h"
#include "progressdispatcher.h"
#include "utility.h"
#include "syncfilestatus.h"

class QProcess;

namespace OCC {

class SyncJournalFileRecord;

class SyncJournalDb;

class OwncloudPropagator;

class OWNCLOUDSYNC_EXPORT SyncEngine : public QObject
{
    Q_OBJECT
public:
    SyncEngine(CSYNC *, const QString &localPath, const QString &remoteURL, const QString &remotePath, SyncJournalDb *journal);
    ~SyncEngine();

    static QString csyncErrorToString( CSYNC_STATUS);

    Q_INVOKABLE void startSync();
    void setNetworkLimits(int upload, int download);

    /* Abort the sync.  Called from the main thread */
    void abort();

    Utility::StopWatch &stopWatch() { return _stopWatch; }

    void setSelectiveSyncBlackList(const QStringList &list);

    /* Return true if we detected that another sync is needed to complete the sync */
    bool isAnotherSyncNeeded() { return _anotherSyncNeeded; }

    bool estimateState(QString fn, csync_ftw_type_e t, SyncFileStatus* s);

    /** Get the ms since a file was touched, or -1 if it wasn't.
     *
     * Thread-safe.
     */
    qint64 timeSinceFileTouched(const QString& fn) const;

signals:
    void csyncError( const QString& );
    void csyncUnavailable();

    // During update, before reconcile
    void folderDiscovered(bool local, QString folderUrl);

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
    void slotDiscoveryJobFinished(int updateResult);
    void slotCleanPollsJobAborted(const QString &error);

private:
    void handleSyncError(CSYNC *ctx, const char *state);

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    bool checkBlacklisting( SyncFileItem *item );

    // Cleans up unnecessary downloadinfo entries in the journal as well
    // as their temporary files.
    void deleteStaleDownloadInfos();

    // Removes stale uploadinfos from the journal.
    void deleteStaleUploadInfos();

    // Removes stale blacklist entries from the journal.
    void deleteStaleBlacklistEntries();

    // cleanup and emit the finished signal
    void finalize();

    static bool _syncRunning; //true when one sync is running somewhere (for debugging)

    QMap<QString, SyncFileItem> _syncItemMap;

    // should be called _syncItems (present tense). It's the items from the _syncItemMap but
    // sorted and re-adjusted based on permissions.
    SyncFileItemVector _syncedItems;

    CSYNC *_csync_ctx;
    bool _needsUpdate;
    QString _localPath;
    QString _remoteUrl;
    QString _remotePath;
    SyncJournalDb *_journal;
    QSharedPointer <OwncloudPropagator> _propagator;
    QString _lastDeleted; // if the last item was a path and it has been deleted
    QSet<QString> _seenFiles;
    QThread _thread;

    Progress::Info _progressInfo;

    Utility::StopWatch _stopWatch;

    // maps the origin and the target of the folders that have been renamed
    QHash<QString, QString> _renamedFolders;
    QString adjustRenamedPath(const QString &original);

    /**
     * check if we are allowed to propagate everything, and if we are not, adjust the instructions
     * to recover
     */
    void checkForPermission();
    QByteArray getPermissions(const QString& file) const;

    bool _hasNoneFiles; // true if there is at least one file with instruction NONE
    bool _hasRemoveFile; // true if there is at leasr one file with instruction REMOVE

    int _uploadLimit;
    int _downloadLimit;

    // hash containing the permissions on the remote directory
    QHash<QString, QByteArray> _remotePerms;

    QStringList _selectiveSyncBlackList;

    bool _anotherSyncNeeded;
};

}

#endif // CSYNCTHREAD_H
