/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SYNCFILESTATUSTRACKER_H
#define SYNCFILESTATUSTRACKER_H

#include "ownsql.h"
#include "syncfileitem.h"
#include "syncfilestatus.h"
#include <QSet>
#include <csync.h>

namespace OCC {

class SyncEngine;

class OWNCLOUDSYNC_EXPORT SyncFileStatusTracker : public QObject
{
    Q_OBJECT
public:
    SyncFileStatusTracker(SyncEngine *syncEngine);
    SyncFileStatus fileStatus(const QString& systemFileName);

signals:
    void fileStatusChanged(const QString& systemFileName, SyncFileStatus fileStatus);

private slots:
    void slotThreadTreeWalkResult(const SyncFileItemVector& items);
    void slotAboutToPropagate(SyncFileItemVector& items);
    void slotSyncFinished();
    void slotItemCompleted(const SyncFileItem &item);
    void slotItemDiscovered(const SyncFileItem &item);

private:
    bool estimateState(QString fn, csync_ftw_type_e t, SyncFileStatus* s);

    SyncEngine *_syncEngine;
    // SocketAPI: Cache files and folders that had errors so that they can
    // get a red ERROR icon.
    QSet<QString>   _stateLastSyncItemsWithErrorNew; // gets moved to _stateLastSyncItemsWithError at end of sync
    QSet<QString>   _stateLastSyncItemsWithError;
};

}

#endif
