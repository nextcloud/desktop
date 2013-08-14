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
#include <qelapsedtimer.h>
#include <QNetworkProxy>
#include <QNetworkCookie>

#include <csync.h>

#include "mirall/syncfileitem.h"
#include "progressdatabase.h"
#include "mirall/progressdispatcher.h"

class QProcess;

Q_DECLARE_METATYPE(CSYNC_ERROR_CODE)

namespace Mirall {

class OwncloudPropagator;


class CSyncThread : public QObject
{
    Q_OBJECT

    // Keep the actions that have been performed, in order to update the db
    struct Action {
        QByteArray etag;
        csync_instructions_e instruction;
    };
    QHash<QByteArray, Action> _performedActions;

public:
    CSyncThread(CSYNC *, const QString &localPath, const QString &remotePath);
    ~CSyncThread();

    static QString csyncErrorToString( CSYNC_ERROR_CODE);

    Q_INVOKABLE void startSync();

signals:
    void csyncError( const QString& );
    void csyncWarning( const QString& );
    void csyncUnavailable();
    void treeWalkResult(const SyncFileItemVector&);

    void transmissionProgress( const Progress::Info& progress );
    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

    void aboutToRemoveAllFiles(SyncFileItem::Direction direction, bool *cancel);

private slots:
    void transferCompleted(const SyncFileItem& item, CSYNC_ERROR_CODE error);
    void startNextTransfer();
    void slotProgress(Progress::Kind kind, const QString& file, quint64, quint64);

private:
    void handleSyncError(CSYNC *ctx, const char *state);

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    int treewalkFinalize( TREE_WALK_FILE* );

    Progress::Kind csyncToProgressKind( enum csync_notify_type_e kind );
    static int walkFinalize(TREE_WALK_FILE*, void* );


    static QMutex _mutex;
    static QMutex _syncMutex;
    SyncFileItemVector _syncedItems;
    int _iterator; // index in _syncedItems for the next item to process.
    ProgressDatabase _progressDataBase;


    CSYNC *_csync_ctx;
    bool _needsUpdate;
    QString _localPath;
    QString _remotePath;
    QScopedPointer <OwncloudPropagator> _propagator;
    QElapsedTimer _syncTime;
    QString _lastDeleted; // if the last item was a path and it has been deleted



    // maps the origin and the target of the folders that have been renamed
    QHash<QString, QString> _renamedFolders;
    QString adjustRenamedPath(const QString &original);

    bool _hasFiles; // true if there is at least one file that is not ignored or removed
    Progress::Info _progressInfo;
    int _downloadLimit;
    int _uploadLimit;

    friend struct CSyncRunScopeHelper;
};
}

#endif // CSYNCTHREAD_H
