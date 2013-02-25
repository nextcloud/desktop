/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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


#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QTimer>

#if QT_VERSION >= 0x040700
#include <QNetworkConfigurationManager>
#endif

#include "mirall/syncresult.h"

class QAction;
class QIcon;
class QFileSystemWatcher;

namespace Mirall {

class FolderWatcher;

class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const QString&, const QString&, const QString& , QObject*parent = 0L);
    virtual ~Folder();

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
    virtual QString secondPath() const;

    /**
     * local folder path with native separators
     */
    QString nativePath() const;
    virtual QString nativeSecondPath() const;
    /**
     * switch sync on or off
     * If the sync is switched off, the startSync method is not going to
     * be called.
     */
     void setSyncEnabled( bool );

     bool syncEnabled() const;

    /**
     * Starts a sync operation
     *
     * If the list of changed files is known, it is passed.
     *
     * If the list of changed files is empty, the folder
     * implementation should figure it by itself of
     * perform a full scan of changes
     */
    virtual void startSync(const QStringList &pathList) = 0;

    /**
     * True if the folder is busy and can't initiate
     * a synchronization
     */
    virtual bool isBusy() const = 0;

    /**
     * only sync when online in the network
     */
    bool onlyOnlineEnabled() const;

    /**
     * @see onlyOnlineEnabled
     */
    void setOnlyOnlineEnabled(bool enabled);

    /**
     * only sync when online in the same LAN
     * as the one used during setup
     */
    bool onlyThisLANEnabled() const;

    /**
     * @see onlyThisLANEnabled
     */
    void setOnlyThisLANEnabled(bool enabled);


    /**
      * error counter, stop syncing after the counter reaches a certain
      * number.
      */
    int errorCount();

    void resetErrorCount();

    void incrementErrorCount();

    /**
     * return the last sync result with error message and status
     */
     SyncResult syncResult() const;

     /**
     * set the backend description string.
     */
     void setBackend( const QString& );
     /**
     * get the backend description string.
     */
     QString backend() const;

     /**
      * set the config file name.
      */
     void setConfigFile( const QString& );
     QString configFile();

     /**
      * This is called if the sync folder definition is removed. Do cleanups here.
      */
     virtual void wipe();

     QIcon icon( int size ) const;
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
     virtual void slotTerminateSync() = 0;

     /**
      * Sets minimum amounts of milliseconds that will separate
      * poll intervals
      */
     void setPollInterval( int );

protected:
    /**
     * The minimum amounts of seconds to wait before
     * doing a full sync to see if the remote changed
     */
    int pollInterval() const;
    void setSyncState(SyncResult::Status state);

    FolderWatcher *_watcher;
    int _errorCount;
    SyncResult _syncResult;

protected slots:

    void slotOnlineChanged(bool online);

    void slotPollTimerTimeout();

    /* called when the watcher detect a list of changed
       paths */

    void slotSyncStarted();

    /**
     * Triggered by a file system watcher on the local sync dir
     */
    virtual void slotLocalPathChanged( const QString& );

private:

    /**
     * Starts a sync (calling startSync)
     * if the policies allow for it
     */
    void evaluateSync(const QStringList &pathList);

    virtual void checkLocalPath();

    QString   _path;
    QString   _secondPath;
    QString   _alias;
    bool      _onlyOnlineEnabled;
    bool      _onlyThisLANEnabled;
    QString   _configFile;

    QFileSystemWatcher *_pathWatcher;

#if QT_VERSION >= 0x040700
    QNetworkConfigurationManager _networkMgr;
#endif
    bool       _online;
    bool       _enabled;
    QString    _backend;

};

}

#endif
