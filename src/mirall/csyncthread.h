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

#ifndef CSYNCTHREAD_H
#define CSYNCTHREAD_H

#include <stdint.h>

#include <QMutex>
#include <QThread>
#include <QString>
#include <QNetworkProxy>

#include <csync.h>

#include "mirall/syncfileitem.h"

class QProcess;

namespace Mirall {

class CSyncThread : public QObject
{
    Q_OBJECT
public:
    CSyncThread(CSYNC *);
    ~CSyncThread();

    QString csyncErrorToString( CSYNC_ERROR_CODE, const char * );

    Q_INVOKABLE void startSync();

signals:
    void fileReceived( const QString& );
    void fileRemoved( const QString& );
    void csyncError( const QString& );
    void csyncWarning( const QString& );
    void csyncUnavailable();
    void treeWalkResult(const SyncFileItemVector&);

    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

private:
    void handleSyncError(CSYNC *ctx, const char *state);
    static void progress(const char *remote_url,
                    enum csync_notify_type_e kind,
                    long long o1, long long o2,
                    void *userdata);

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    int treewalkError( TREE_WALK_FILE* );

    static int walkFinalize(TREE_WALK_FILE*, void* );



    static QMutex _mutex;
    static QMutex _syncMutex;
    SyncFileItemVector _syncedItems;

    CSYNC *_csync_ctx;
    bool _needsUpdate;

    friend class CSyncRunScopeHelper;
};
}

#endif // CSYNCTHREAD_H
