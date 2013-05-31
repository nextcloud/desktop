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

#ifndef MIRALL_ownCloudFolder_H
#define MIRALL_ownCloudFolder_H

#include <QMutex>
#include <QThread>
#include <QStringList>

#include "mirall/folder.h"
#include "mirall/csyncthread.h"

class QProcess;
class QTimer;

namespace Mirall {

enum SyncFileStatus_s {
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
};
typedef SyncFileStatus_s SyncFileStatus;

class ServerActionNotifier : public QObject
{
    Q_OBJECT
public:
    ServerActionNotifier(QObject *parent = 0);
public slots:
    void slotSyncFinished(const SyncResult &result);
signals:
    void guiLog(const QString&, const QString&);
    void sendResults();
private:
};

class ownCloudFolder : public Folder
{
    Q_OBJECT
public:
    ownCloudFolder(const QString &alias,
                   const QString &path,
                   const QString &secondPath, QObject *parent = 0L);
    virtual ~ownCloudFolder();
    QString secondPath() const;
    virtual bool isBusy() const;
    virtual void startSync(const QStringList &pathList);

    virtual void wipe();

    /* get status about a singel file. */
    SyncFileStatus fileStatus( const QString& );

    void setProxy();

public slots:
    void startSync();
    void slotTerminateSync();

protected slots:
    void slotLocalPathChanged( const QString& );
    void slotThreadTreeWalkResult(const SyncFileItemVector& );

private slots:
    void slotCSyncStarted();
    void slotCSyncError(const QString& );
    void slotCsyncUnavailable();
    void slotCSyncFinished();

private:
    static int getauth(const char *prompt,
                             char *buf,
                             size_t len,
                             int echo,
                             int verify,
                             void *userdata
                             );
    const char* proxyTypeToCStr(QNetworkProxy::ProxyType type);

    QString      _secondPath;
    QThread     *_thread;
    CSyncThread *_csync;
    QStringList  _errors;
    bool         _csyncError;
    bool         _csyncUnavail;
    bool         _wipeDb;
    SyncFileItemVector _items;

    CSYNC *_csync_ctx;
};

}

#endif
