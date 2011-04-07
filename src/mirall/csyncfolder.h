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

#ifndef MIRALL_CSYNCFOLDER_H
#define MIRALL_CSYNCFOLDER_H

#include <QMutex>
#include <QThread>
#include <QStringList>

#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class CSyncThread : public QThread
{
public:
    CSyncThread(const QString &source, const QString &target);
    ~CSyncThread();

    virtual void run();

private:
    QString _source;
    QString _target;
};

class CSyncFolder : public Folder
{
    Q_OBJECT
public:
  CSyncFolder(const QString &alias,
               const QString &path,
               const QString &secondPath, QObject *parent = 0L);
    virtual ~CSyncFolder();
    QString secondPath() const;
    virtual void startSync(const QStringList &pathList);
    virtual bool isBusy() const;
protected slots:
    void slotCSyncStarted();
    void slotCSyncFinished();
private:
    QString _secondPath;
};

}

#endif
