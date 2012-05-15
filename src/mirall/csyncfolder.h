#ifndef CSYNCFOLDER_H
#define CSYNCFOLDER_H

/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include <QMutex>
#include <QThread>
#include <QString>

#include "mirall/csyncthread.h"
#include "mirall/folder.h"

namespace Mirall {


class CSyncFolder : public Folder
{
    Q_OBJECT
public:
    CSyncFolder(const QString &alias,
                const QString &path,
                const QString &secondPath, QObject *parent = 0L);
    virtual ~CSyncFolder();
    virtual void startSync(const QStringList &pathList);
    virtual bool isBusy() const;

public slots:
    void slotTerminateSync();

protected slots:
    void slotCSyncStarted();
    void slotCSyncFinished();
    void slotCSyncError( const QString& );
private:
    bool    _csyncError;
    CSyncThread *_csync;
    QStringList _errors;
};

}

#endif
