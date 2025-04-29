/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNCFILESTATUSTRACKER_H
#define SYNCFILESTATUSTRACKER_H

// #include "ownsql.h"
#include "syncfileitem.h"
#include "common/syncfilestatus.h"
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
    explicit SyncFileStatusTracker(SyncEngine *syncEngine);
    SyncFileStatus fileStatus(const QString &relativePath);

public slots:
    void slotPathTouched(const QString &fileName);
    // path relative to folder
    void slotAddSilentlyExcluded(const QString &folderPath);
    void slotCheckAndRemoveSilentlyExcluded(const QString &folderPath);

signals:
    void fileStatusChanged(const QString &systemFileName, OCC::SyncFileStatus fileStatus);

private slots:
    void slotAboutToPropagate(OCC::SyncFileItemVector &items);
    void slotItemCompleted(const OCC::SyncFileItemPtr &item);
    void slotSyncFinished();
    void slotSyncEngineRunningChanged();

private:
    struct PathComparator {
        bool operator()( const QString& lhs, const QString& rhs ) const;
    };
    using ProblemsMap = std::map<QString, SyncFileStatus::SyncFileStatusTag, PathComparator>;
    SyncFileStatus::SyncFileStatusTag lookupProblem(const QString &pathToMatch, const ProblemsMap &problemMap);

    enum SharedFlag { UnknownShared,
        NotShared,
        Shared };
    enum PathKnownFlag { PathUnknown = 0,
        PathKnown };
    SyncFileStatus resolveSyncAndErrorStatus(const QString &relativePath, SharedFlag sharedState, PathKnownFlag isPathKnown = PathKnown);

    void invalidateParentPaths(const QString &path);
    QString getSystemDestination(const QString &relativePath);
    void incSyncCountAndEmitStatusChanged(const QString &relativePath, SharedFlag sharedState);
    void decSyncCountAndEmitStatusChanged(const QString &relativePath, SharedFlag sharedState);

    SyncEngine *_syncEngine;

    ProblemsMap _syncProblems;
    ProblemsMap _syncSilentExcludes;
    QSet<QString> _dirtyPaths;
    // Counts the number direct children currently being synced (has unfinished propagation jobs).
    // We'll show a file/directory as SYNC as long as its sync count is > 0.
    // A directory that starts/ends propagation will in turn increase/decrease its own parent by 1.
    QHash<QString, int> _syncCount;
};
}

#endif
