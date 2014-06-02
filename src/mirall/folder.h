/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_FOLDER_H
#define MIRALL_FOLDER_H

#include "mirall/syncresult.h"
#include "mirall/progressdispatcher.h"
#include "mirall/syncjournaldb.h"
#include "mirall/clientproxy.h"

#include <csync.h>

#include <QDir>
#include <QHash>
#include <QObject>
#include <QStringList>

#include <QDebug>
#include <QTimer>
#include <qelapsedtimer.h>

class QFileSystemWatcher;
class QThread;

namespace Mirall {

class SyncEngine;

class FolderWatcher;


typedef enum SyncFileStatus_s {
    FILE_STATUS_NONE,
    FILE_STATUS_EVAL,
    FILE_STATUS_REMOVE,
    FILE_STATUS_RENAME,
    FILE_STATUS_MOVE,
    FILE_STATUS_NEW,
    FILE_STATUS_CONFLICT,
    FILE_STATUS_IGNORE,
    FILE_STATUS_SYNC,
    FILE_STATUS_STAT_ERROR,
    FILE_STATUS_ERROR,
    FILE_STATUS_UPDATED
} SyncFileStatus;


class OWNCLOUDSYNC_EXPORT Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const QString&, const QString&, const QString& , QObject*parent = 0L);

    ~Folder();

    typedef QHash<QString, Folder*> Map;
    typedef QHashIterator<QString, Folder*> MapIterator;

    /**
     * Get status about a single file.
     */
    SyncFileStatus fileStatus( const QString& );

    /**
     * @brief recursiveFolderStatus
     * @param fileName - the relative file name to examine
     * @return the resulting status
     *
     * The resulting status can only be either SYNC which means all files
     * are in sync, ERROR if an error occured, or EVAL if something needs
     * to be synced underneath this dir.
     */
    SyncFileStatus recursiveFolderStatus( const QString& fileName );

    /**
     * alias or nickname
     */
    QString alias() const;

    /**
     * local folder path
     */
    QString path() const;

    /**
     * remote folder path
     */
    QString remotePath() const;

    /**
     * remote folder path with server url
     */
    QUrl remoteUrl() const;

    /**
     * local folder path with native separators
     */
    QString nativePath() const;

    /**
     * switch sync on or off
     * If the sync is switched off, the startSync method is not going to
     * be called.
     */
     void setSyncEnabled( bool );

     bool syncEnabled() const;

     void prepareToSync();

    /**
     * True if the folder is busy and can't initiate
     * a synchronization
     */
    virtual bool isBusy() const;

    /**
     * return the last sync result with error message and status
     */
     SyncResult syncResult() const;

     /**
      * set the config file name.
      */
     void setConfigFile( const QString& );
     QString configFile();

     /**
      * This is called if the sync folder definition is removed. Do cleanups here.
      */
     virtual void wipe();

     void setSyncState(SyncResult::Status state);

     void setDirtyNetworkLimits();

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync( const QString& );

public slots:

     /**
       * terminate the current sync run
       */
     void slotTerminateSync();

     void slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool*);


     /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
      void startSync(const QStringList &pathList = QStringList());

      void setProxyDirty(bool value);
      bool proxyDirty();

      int slotWipeBlacklist();
      int blackListEntryCount();

private slots:
    void slotSyncStarted();
    void slotSyncError(const QString& );
    void slotCsyncUnavailable();
    void slotSyncFinished();

    void slotTransmissionProgress(const Progress::Info& pi);

    void slotPollTimerTimeout();
    void etagRetreived(const QString &);
    void slotNetworkUnavailable();

    void slotThreadTreeWalkResult(const SyncFileItemVector& );

    void slotEmitFinishedDelayed();

private:
    bool init();

    void setIgnoredFiles();

    void bubbleUpSyncResult();

    void checkLocalPath();

    void createGuiLog(const QString& filename, SyncFileStatus status, int count,
                       const QString& renameTarget = QString::null );

    QString   _path;
    QString   _remotePath;
    QString   _alias;
    QString   _configFile;
    QFileSystemWatcher *_pathWatcher;
    bool       _enabled;
    SyncResult _syncResult;
    QScopedPointer<SyncEngine> _engine;
    QStringList  _errors;
    bool         _csyncError;
    bool         _csyncUnavail;
    bool         _wipeDb;
    bool         _proxyDirty;
    QTimer        _pollTimer;
    QString       _lastEtag;
    QElapsedTimer _timeSinceLastSync;

    SyncJournalDb _journal;

    ClientProxy   _clientProxy;

    CSYNC *_csync_ctx;
};

}

#endif
