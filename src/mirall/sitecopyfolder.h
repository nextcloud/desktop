/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#ifndef MIRALL_SiteCopyFOLDER_H
#define MIRALL_SiteCopyFOLDER_H

#include <QMutex>
#include <QProcess>
#include <QStringList>

#include "mirall/folder.h"

#define SITECOPY_BIN "/usr/bin/sitecopy"

class QProcess;

namespace Mirall {

class SiteCopyFolder : public Folder
{
    Q_OBJECT
public:
    enum SiteCopyState { Sync, Update, Finish, Status, FlatList, ExecuteStatus };

    SiteCopyFolder(const QString &alias,
                   const QString &path,
                   const QString &secondPath, QObject *parent = 0L);
    virtual ~SiteCopyFolder();

    virtual void startSync(const QStringList &pathList);

    // load data from ownCloud to the local directory.
    void fetchFromOC();
    
    // push data from the local directory to ownCloud
    void pushToOC();

    virtual bool isBusy() const;

    QString remotePath() const;

signals:
    void statusString( const QString& );

protected slots:
    void slotReadyReadStandardOutput();
    void slotReadyReadStandardError();
    void slotStateChanged(QProcess::ProcessState);
    void slotFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotStarted();
    void slotError(QProcess::ProcessError);

    void startSiteCopy( const QString&, SiteCopyState );
    void analyzeStatus();

private:
    QMutex      _syncMutex;
    QProcess   *_SiteCopy;
    int         _syncCount;
    bool        _pathListEmpty;

    QByteArray    _lastOutput;
    QString       _StatusString;
    QString       _remotePath;
    SiteCopyState _NextStep;
    QString       _siteCopyAlias;
    QHash<QString, QStringList> _ChangesHash;

};

}

#endif
