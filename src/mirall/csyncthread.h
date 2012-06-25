#ifndef CSYNCTHREAD_H
#define CSYNCTHREAD_H

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

#include <stdint.h>

#include <QMutex>
#include <QThread>
#include <QString>

#include <csync.h>

class QProcess;

namespace Mirall {

enum walkErrorTypes {
    WALK_ERROR_NONE = 0,
    WALK_ERROR_WALK,
    WALK_ERROR_INSTRUCTIONS,
    WALK_ERROR_DIR_PERMS
};

struct walkStats_s {
    const char *sourcePath;
    int errorType;

    ulong eval;
    ulong removed;
    ulong renamed;
    ulong newFiles;
    ulong conflicts;
    ulong ignores;
    ulong sync;
    ulong error;

    ulong seenFiles;
};

typedef walkStats_s WalkStats;

class CSyncThread : public QThread
{
    Q_OBJECT
public:
    CSyncThread(const QString &source, const QString &target, bool = false);
    ~CSyncThread();

    virtual void run();

    static void setUserPwd( const QString&, const QString& );
    static int checkPermissions( TREE_WALK_FILE* file, void *data);
    static QString csyncConfigDir();

signals:
    void treeWalkResult(WalkStats*);
    void csyncError( const QString& );

    void csyncStateDbFile( const QString& );
    void wipeDb();

private:
    void emitStateDb( CSYNC *csync );

    static int getauth(const char *prompt,
                char *buf,
                size_t len,
                int echo,
                int verify,
                void *userdata
    );

    static QMutex _mutex;
    static QString _user;
    static QString _passwd;
    static QString _csyncConfigDir;

    QString _source;
    QString _target;
    bool    _localCheckOnly;
};
}

#endif // CSYNCTHREAD_H
