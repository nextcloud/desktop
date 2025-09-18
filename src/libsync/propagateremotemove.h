/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudpropagator.h"
#include "networkjobs.h"

namespace OCC {

/**
 * @brief The MoveJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT MoveJob : public AbstractNetworkJob
{
    Q_OBJECT
    const QString _destination;
    const QUrl _url; // Only used (instead of path) when the constructor taking an URL is used
    QMap<QByteArray, QByteArray> _extraHeaders;

public:
    explicit MoveJob(AccountPtr account, const QString &path, const QString &destination, QObject *parent = nullptr);
    explicit MoveJob(AccountPtr account, const QUrl &url, const QString &destination,
        QMap<QByteArray, QByteArray> _extraHeaders, QObject *parent = nullptr);

    void start() override;
    bool finished() override;

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
    void start() override;
    void abort(PropagatorJob::AbortType abortType) override;
    [[nodiscard]] JobParallelism parallelism() const override { return _item->isDirectory() ? WaitForFinished : FullParallelism; }

    /**
     * Rename the directory in the selective sync list
     */
    static bool adjustSelectiveSync(SyncJournalDb *journal, const QString &from, const QString &to);

private slots:
    void slotMoveJobFinished();
    void finalize();
};
}
