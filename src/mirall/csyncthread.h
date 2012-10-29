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
#include <QNetworkProxy>

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
    walkStats_s();

    int errorType;

    ulong eval;
    ulong removed;
    ulong renamed;
    ulong newFiles;
    ulong conflicts;
    ulong ignores;
    ulong sync;
    ulong error;

    ulong dirPermErrors;

    ulong seenFiles;
};

typedef walkStats_s WalkStats;

struct syncFileItem_s {
    QString file;
    csync_instructions_e instruction;
};
typedef syncFileItem_s SyncFileItem;

typedef QVector<SyncFileItem> SyncFileItemVector;

class CSyncThread : public QObject
{
    Q_OBJECT
public:
    CSyncThread(const QString &source, const QString &target, bool = false);
    ~CSyncThread();

    static void setConnectionDetails( const QString&, const QString&, const QNetworkProxy& );
    static QString csyncConfigDir();

    Q_INVOKABLE void startSync();

signals:
    void treeWalkResult(const SyncFileItemVector&, const WalkStats&);
    void csyncError( const QString& );

    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

private:
    static int treewalk( TREE_WALK_FILE* file, void *data );
    int recordStats( TREE_WALK_FILE* file);
    void emitStateDb( CSYNC *csync );
    int treewalkFile( TREE_WALK_FILE* );

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
    static QNetworkProxy _proxy;

    static QString _csyncConfigDir;

    QString _source;
    QString _target;
    bool    _localCheckOnly;

    QVector <SyncFileItem> _syncedItems;
    WalkStats _walkStats;
};
}

#endif // CSYNCTHREAD_H
