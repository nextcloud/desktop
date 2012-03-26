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

#ifndef MIRALL_GITFOLDER_H
#define MIRALL_GITFOLDER_H

#include <QMutex>
#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class GitFolder : public Folder
{
Q_OBJECT
public:
    /**
     * path : Local folder to be keep in sync
     * remote: git repo url to sync from/to
     */
  GitFolder(const QString &alias,
            const QString &path,
            const QString &secondPath, QObject *parent = 0L);
    virtual ~GitFolder();

    virtual void startSync();
private:
    QMutex _syncMutex;
    QProcess *_syncProcess;
};

}

#endif
