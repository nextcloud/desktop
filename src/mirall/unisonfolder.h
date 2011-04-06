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

#ifndef MIRALL_UNISONFOLDER_H
#define MIRALL_UNISONFOLDER_H

#include <QMutex>
#include <QProcess>
#include <QStringList>

#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class UnisonFolder : public Folder
{
    Q_OBJECT
public:
  UnisonFolder(const QString &alias,
               const QString &path,
               const QString &secondPath, QObject *parent = 0L);
    virtual ~UnisonFolder();

    QString secondPath() const;

    virtual void startSync(const QStringList &pathList);

    virtual bool isBusy() const;

protected slots:
    void slotReadyReadStandardOutput();
    void slotReadyReadStandardError();
    void slotStateChanged(QProcess::ProcessState);
    void slotFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotStarted();
    void slotError(QProcess::ProcessError);
private:
    QMutex _syncMutex;
    QProcess *_unison;
    QString _secondPath;
    int _syncCount;

    QString _lastOutput;

};

}

#endif
