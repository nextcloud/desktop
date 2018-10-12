/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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
#pragma once

#include "owncloudpropagator.h"
#include "networkjobs.h"

namespace OCC {

/**
 * @brief The MoveJob class
 * @ingroup libsync
 */
class MoveJob : public AbstractNetworkJob
{
    Q_OBJECT
    const QString _destination;
    const QUrl _url; // Only used (instead of path) when the constructor taking an URL is used
    QMap<QByteArray, QByteArray> _extraHeaders;

public:
    explicit MoveJob(AccountPtr account, const QString &path, const QString &destination, QObject *parent = Q_NULLPTR);
    explicit MoveJob(AccountPtr account, const QUrl &url, const QString &destination,
        QMap<QByteArray, QByteArray> _extraHeaders, QObject *parent = Q_NULLPTR);

    void start() Q_DECL_OVERRIDE;
    bool finished() Q_DECL_OVERRIDE;

signals:
    void finishedSignal();
};

/**
 * @brief The PropagateRemoteMove class
 * @ingroup libsync
 */
class PropagateRemoteMove : public PropagateItemJob
{
    Q_OBJECT
    QPointer<MoveJob> _job;

public:
    PropagateRemoteMove(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() Q_DECL_OVERRIDE;
    void abort(PropagatorJob::AbortType abortType) Q_DECL_OVERRIDE;
    JobParallelism parallelism() Q_DECL_OVERRIDE { return _item->isDirectory() ? WaitForFinished : FullParallelism; }

    /**
     * Rename the directory in the selective sync list
     */
    static bool adjustSelectiveSync(SyncJournalDb *journal, const QString &from, const QString &to);

private slots:
    void slotMoveJobFinished();
    void finalize();
};
}
