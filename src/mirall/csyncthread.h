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

struct walkStats_s {
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

    /**
     * true if last operation ended with error
     */
    bool error() const;

    bool hasLocalChanges( int64_t ) const;
    int64_t walkedFiles();

    static void setUserPwd( const QString&, const QString& );
    static QString _user;
    static QString _passwd;
    static int checkPermissions( TREE_WALK_FILE* file, void *data);

signals:
    void treeWalkResult(WalkStats*);

private:
    static int getauth(const char *prompt,
                char *buf,
                size_t len,
                int echo,
                int verify,
                void *userdata
    );

    static QMutex _mutex;
    QString _source;
    QString _target;
    int     _error;
    bool    _localCheckOnly;
};
}

#endif // CSYNCTHREAD_H
