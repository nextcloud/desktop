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

#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <QDebug>

class QFileSystemWatcher;

namespace Mirall {

class FolderWatcher;

typedef enum SyncFileStatus_s {
    STATUS_NONE,
    STATUS_EVAL,
    STATUS_REMOVE,
    STATUS_RENAME,
    STATUS_NEW,
    STATUS_CONFLICT,
    STATUS_IGNORE,
    STATUS_SYNC,
    STATUS_STAT_ERROR,
    STATUS_ERROR,
    STATUS_UPDATED
} SyncFileStatus;

class ServerActionNotifier : public QObject
{
    Q_OBJECT
public:
    ServerActionNotifier(QObject *parent = 0);
public slots:
    void slotSyncFinished(const SyncResult &result);
signals:
    void guiLog(const QString&, const QString&);

private:
};

class Folder : public QObject
{
    Q_OBJECT

protected:
    friend class FolderMan;
    Folder(const QString&, const QString&, const QString& , QObject*parent = 0L);

public:
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
    QString secondPath() const;

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

     QTimer   *_pollTimer;

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync( const QString& );

public slots:
     void slotSyncFinished(const SyncResult &);

     /**
       *
       */
     void slotChanged(const QStringList &pathList = QStringList() );

     /**
       * terminate the current sync run
       */
     void slotTerminateSync();

     /**
      * Sets minimum amounts of milliseconds that will separate
      * poll intervals
      */
     void setPollInterval( int );

     void slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool*);


     /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
      void startSync(const QStringList &pathList = QStringList());

private slots:
    void slotCSyncStarted();
    void slotCSyncError(const QString& );
    void slotCsyncUnavailable();
    void slotCSyncFinished();

    void slotTransmissionProgress(const Progress::Info& progress);

    void slotPollTimerTimeout();


    /** called when the watcher detect a list of changed paths */

    void slotSyncStarted();

    /**
     * Triggered by a file system watcher on the local sync dir
     */
    void slotLocalPathChanged( const QString& );
    void slotThreadTreeWalkResult(const SyncFileItemVector& );

protected:
    bool init();

    /**
     * The minimum amounts of seconds to wait before
     * doing a full sync to see if the remote changed
     */
    int pollInterval() const;
    void setSyncState(SyncResult::Status state);

    void setIgnoredFiles();
    void setProxy();
    const char* proxyTypeToCStr(QNetworkProxy::ProxyType type);

    /**
     * Starts a sync (calling startSync)
     * if the policies allow for it
     */
    void evaluateSync(const QStringList &pathList);

    void checkLocalPath();

    QString   _path;
    QString   _secondPath;
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
    Progress::Kind _progressKind;

    CSYNC *_csync_ctx;
};

}

#endif
