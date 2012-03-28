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
#include <QMutex>
#include <QThread>
#include <QStringList>

#include "mirall/folder.h"
#include "mirall/csyncthread.h"

class QProcess;

namespace Mirall {

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

public slots:
    void startSync();

private slots:
    void slotCSyncStarted();
    void slotCSyncError(const QString& );
    void slotCSyncFinished();
    void slotThreadTreeWalkResult( WalkStats* );
    void slotCSyncTerminated();

#ifndef USE_WATCHER
    void slotPollTimerRemoteCheck();
#endif
private:
    QString      _secondPath;
    CSyncThread *_csync;
    bool         _localCheckOnly;
    int          _pollTimerCnt;
    QStringList  _errors;
    bool         _csyncError;
    ulong        _lastSeenFiles;
};

}

#endif
