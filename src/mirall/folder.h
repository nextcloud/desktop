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
#include "mirall/csyncthread.h"
#include "mirall/syncjournaldb.h"

#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QObject>
#include <QStringList>

#include <QDebug>
#include <QTimer>
#include <qelapsedtimer.h>

class QFileSystemWatcher;
class QThread;

namespace Mirall {

class FolderWatcher;

typedef enum SyncFileStatus_s {
    FILE_STATUS_NONE,
    FILE_STATUS_EVAL,
    FILE_STATUS_REMOVE,
    FILE_STATUS_RENAME,
    FILE_STATUS_NEW,
    FILE_STATUS_CONFLICT,
    FILE_STATUS_IGNORE,
    FILE_STATUS_SYNC,
    FILE_STATUS_STAT_ERROR,
    FILE_STATUS_ERROR,
    FILE_STATUS_UPDATED
} SyncFileStatus;


class Folder : public QObject
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

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync( const QString& );

public slots:

     /**
       *
       */
     void slotChanged(const QStringList &pathList = QStringList() );

     /**
       * terminate the current sync run
       *
       * If block is true, this will block synchroniously for the sync thread to finish.
       */
     void slotTerminateSync(bool block);

     void slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool*);


     /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
      void startSync(const QStringList &pathList = QStringList());

      /**
       * Starts a sync (calling startSync)
       * if the policies allow for it
       */
      void evaluateSync(const QStringList &pathList);

      void setProxyDirty(bool value);
      bool proxyDirty();

      int slotWipeBlacklist();
      int blackListEntryCount();

private slots:
    void slotCSyncStarted();
    void slotCSyncError(const QString& );
    void slotCsyncUnavailable();
    void slotCSyncFinished();

    void slotTransmissionProgress(const Progress::Info& progress);
    void slotTransmissionProblem( const Progress::SyncProblem& problem );

    void slotPollTimerTimeout();
    void etagRetreived(const QString &);
    void slotNetworkUnavailable();

    /**
     * Triggered by a file system watcher on the local sync dir
     */
    void slotLocalPathChanged( const QString& );
    void slotThreadTreeWalkResult(const SyncFileItemVector& );
    void slotCatchWatcherError( const QString& );

private:
    bool init();

    void setSyncState(SyncResult::Status state);

    void setIgnoredFiles();
    void setProxy();
    const char* proxyTypeToCStr(QNetworkProxy::ProxyType type);

    void bubbleUpSyncResult();

    void checkLocalPath();

    void createGuiLog( const QString& filename, const QString& verb, int count );

    QString   _path;
    QString   _remotePath;
    QString   _alias;
    QString   _configFile;
    QFileSystemWatcher *_pathWatcher;
    bool       _enabled;
    FolderWatcher *_watcher;
    SyncResult _syncResult;
    QThread     *_thread;
    CSyncThread *_csync;
    QStringList  _errors;
    bool         _csyncError;
    bool         _csyncUnavail;
    bool         _wipeDb;
    bool         _proxyDirty;
    Progress::Kind _progressKind;
    QTimer        _pollTimer;
    QString       _lastEtag;
    QElapsedTimer _timeSinceLastSync;

    SyncJournalDb _journal;

    CSYNC *_csync_ctx;

    const char *_proxy_type;
    QByteArray  _proxy_host;
    int         _proxy_port;
    QByteArray  _proxy_user;
    QByteArray  _proxy_pwd;

};

}

#endif
