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

#include <QNetworkConfigurationManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

#include "mirall/syncresult.h"


/*
 * Flag to enable the folder watcher instead of the local polling to detect
 * changes in the local path.
 */
#define USE_WATCHER 1
#ifdef Q_WS_WIN
#undef USE_WATCHER
#endif

class QAction;
class QTimer;
class QIcon;

namespace Mirall {

#ifdef USE_WATCHER
class FolderWatcher;
#endif

class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const QString &alias, const QString &path, QObject *parent = 0L);
    virtual ~Folder();

    typedef QHash<QString, Folder*> Map;

    /**
     * alias or nickname
     */
    QString alias() const;

    /**
     * local folder path
     */
    QString path() const;

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

     QIcon icon( int size ) const;
  QTimer   *_pollTimer;

public slots:
     void slotSyncFinished(const SyncResult &);
     void slotChanged(const QStringList &pathList = QStringList() );

protected:
    /**
     * The minimum amounts of seconds to wait before
     * doing a full sync to see if the remote changed
     */
    int pollInterval() const;

    /**
     * Sets minimum amounts of milliseconds that will separate
     * poll intervals
     */
    void setPollInterval( int );

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void scheduleToSync(Folder*);

protected:
#ifdef USE_WATCHER
    FolderWatcher *_watcher;
#endif
  int _errorCount;


private:

    /**
     * Starts a sync (calling startSync)
     * if the policies allow for it
     */
    void evaluateSync(const QStringList &pathList);

    QString   _path;

    QString   _alias;
    bool      _onlyOnlineEnabled;
    bool      _onlyThisLANEnabled;
    QNetworkConfigurationManager _networkMgr;
    bool       _online;
    bool       _enabled;
    SyncResult _syncResult;
    QString    _backend;

protected slots:

    void slotOnlineChanged(bool online);

    void slotPollTimerTimeout();

    /* called when the watcher detect a list of changed
       paths */

    void slotSyncStarted();

};

}

#endif
