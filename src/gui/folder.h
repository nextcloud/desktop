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

#include "syncresult.h"
#include "progressdispatcher.h"
#include "syncjournaldb.h"
#include "clientproxy.h"
#include "syncfilestatus.h"

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

class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const QString&, const QString&, const QString& , QObject*parent = 0L);

    ~Folder();

    typedef QHash<QString, Folder*> Map;
    typedef QHashIterator<QString, Folder*> MapIterator;

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
     void setSyncPaused( bool );

     bool syncPaused() const;

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

     // Used by the Socket API
     SyncJournalDb *journalDb() { return &_journal; }
     CSYNC *csyncContext() { return _csync_ctx; }

     QStringList selectiveSyncBlackList() { return _selectiveSyncBlackList; }
     void setSelectiveSyncBlackList(const QStringList &blackList)
     { _selectiveSyncBlackList = blackList; }


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

    void slotFolderDiscovered(bool local, QString folderName);
    void slotTransmissionProgress(const Progress::Info& pi);
    void slotJobCompleted(const SyncFileItem&);
    void slotSyncItemDiscovered(const SyncFileItem & item);

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
    bool       _paused;
    SyncResult _syncResult;
    QScopedPointer<SyncEngine> _engine;
    QStringList  _errors;
    QStringList _selectiveSyncBlackList;
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
