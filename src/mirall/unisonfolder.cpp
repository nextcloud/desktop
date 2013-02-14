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

#include "mirall/unisonfolder.h"

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTextStream>

namespace Mirall {

    UnisonFolder::UnisonFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
      : Folder(alias, path, secondPath, parent),
      _unison(new QProcess(this)),
      _syncCount(0)
{
    QObject::connect(_unison, SIGNAL(readyReadStandardOutput()),
                     SLOT(slotReadyReadStandardOutput()));

    QObject::connect(_unison, SIGNAL(readyReadStandardError()),
                     SLOT(slotReadyReadStandardError()));

    QObject::connect(_unison, SIGNAL(stateChanged(QProcess::ProcessState)),
                     SLOT(slotStateChanged(QProcess::ProcessState)));

    QObject::connect(_unison, SIGNAL(error(QProcess::ProcessError)),
                     SLOT(slotError(QProcess::ProcessError)));

    QObject::connect(_unison, SIGNAL(started()),
                     SLOT(slotStarted()));

    QObject::connect(_unison, SIGNAL(finished(int, QProcess::ExitStatus)),
                     SLOT(slotFinished(int, QProcess::ExitStatus)));
}

UnisonFolder::~UnisonFolder()
{
}

bool UnisonFolder::isBusy() const
{
    return (_unison->state() != QProcess::NotRunning);
}

void UnisonFolder::startSync(const QStringList &pathList)
{
    QMutexLocker locker(&_syncMutex);
    _syncResult.setStatus( SyncResult::SyncRunning );
    emit syncStateChange();

    emit syncStarted();

    QString program = QLatin1String("unison");
    QStringList args;
    args << QLatin1String("-ui") << QLatin1String("text");
    args << QLatin1String("-auto") << QLatin1String("-batch");

    args << QLatin1String("-confirmbigdel=false");

    // only use -path in after a full synchronization
    // already happened, which we do only on the first
    // sync when the program is started
    if (_syncCount > 0 ) {
        // may be we should use a QDir in the API itself?
        QDir root(path());
        foreach( const QString& changedPath, pathList) {
            args << QLatin1String("-path") << root.relativeFilePath(changedPath);
        }
    }

    args  << path();
    args  << secondPath();

    qDebug() << "    * Unison: will use" << pathList.size() << "path arguments";
    _unison->start(program, args);
}

void UnisonFolder::slotTerminateSync()
{
    if( _unison )
        _unison->terminate();
}

void UnisonFolder::slotStarted()
{
    qDebug() << "    * Unison process started ( PID " << _unison->pid() << ")";
    _syncCount++;

    //qDebug() << _unison->readAllStandardOutput();;
}

void UnisonFolder::slotFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "    * Unison process finished with status" << exitCode;

    //if (exitCode != 0) {
    qDebug() << _lastOutput;
        //}

    // parse a summary from here:
    //[BGN] Copying zw.png from //piscola//space/store/folder1 to /space/mirall/folder1
    //[BGN] Deleting gn.png from /space/mirall/folder1
    //[END] Deleting gn.png

    // from stderr:
    //Reconciling changes
    //         <---- new file   Package.h

    _lastOutput.clear();

    emit syncFinished((exitCode != 0) ?
                      SyncResult(SyncResult::Error)
                      : SyncResult(SyncResult::Success));
}

void UnisonFolder::slotReadyReadStandardOutput()
{
    QTextStream stream(&_lastOutput);
    stream << _unison->readAllStandardOutput();;
}

void UnisonFolder::slotReadyReadStandardError()
{
    QTextStream stream(&_lastOutput);
    stream << _unison->readAllStandardError();;
}

void UnisonFolder::slotStateChanged(QProcess::ProcessState state)
{
    //qDebug() << "changed: " << state;
}

void UnisonFolder::slotError(QProcess::ProcessError error)
{
    //qDebug() << "error: " << error;
}

} // ns

