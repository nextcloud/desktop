/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Jocelyn Turcotte <jturcotte@woboq.com>
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
#include <map>
#include <QSet>

namespace OCC {

class SyncEngine;

/**
 * @brief Takes care of tracking the status of individual files as they
 *        go through the SyncEngine, to be reported as overlay icons in the shell.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncFileStatusTracker : public QObject
{
    Q_OBJECT
public:
    explicit SyncFileStatusTracker(SyncEngine* syncEngine);
    SyncFileStatus fileStatus(const QString& relativePath);

public slots:
    void slotPathTouched(const QString& fileName);

signals:
    void fileStatusChanged(const QString& systemFileName, SyncFileStatus fileStatus);

private slots:
    void slotAboutToPropagate(SyncFileItemVector& items);
    void slotItemCompleted(const SyncFileItem& item);
    void slotSyncEngineRunningChanged();
    void slotClearDirtyPaths();

private:
    SyncFileStatus syncFileItemStatus(const SyncFileItem& item);
    SyncFileItem rootSyncFileItem();

    void invalidateParentPaths(const QString& path);
    QString getSystemDestination(const QString& relativePath);

    SyncEngine* _syncEngine;

    std::map<QString, SyncFileStatus::SyncFileStatusTag> _syncProblems;
    QSet<QString> _dirtyPaths;
};

}

#endif
