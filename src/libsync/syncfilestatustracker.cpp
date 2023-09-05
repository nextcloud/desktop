/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Jocelyn Turcotte <jturcotte@woboq.com>
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

#include "syncfilestatustracker.h"
#include "common/asserts.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "syncengine.h"

#include <QFileInfo>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcStatusTracker, "sync.statustracker", QtInfoMsg)

bool SyncFileStatusTracker::PathComparator::operator()( const QString& lhs, const QString& rhs ) const
{
    // This will make sure that the std::map is ordered and queried case-insensitively on macOS and Windows.
    // we want don't want to pay for the runtime check on every comparison.
    static const auto sensitivity = Utility::fsCaseSensitivity();
    return lhs.compare(rhs, sensitivity) < 0;
}

SyncFileStatus::SyncFileStatusTag SyncFileStatusTracker::lookupProblem(const QString &pathToMatch, const SyncFileStatusTracker::ProblemsMap &problemMap)
{
    auto lower = problemMap.lower_bound(pathToMatch);
    for (auto it = lower; it != problemMap.cend(); ++it) {
        const QString &problemPath = it->first;
        SyncFileStatus::SyncFileStatusTag severity = it->second;

        if (problemPath.compare(pathToMatch, _caseSensitivity) == 0) {
            return severity;
        } else if (severity == SyncFileStatus::StatusError
            && problemPath.startsWith(pathToMatch, _caseSensitivity)
            && (pathToMatch.isEmpty() || problemPath.at(pathToMatch.size()) == QLatin1Char('/'))) {
            return SyncFileStatus::StatusWarning;
        } else if (!problemPath.startsWith(pathToMatch, _caseSensitivity)) {
            // Starting at lower_bound we get the first path that is not smaller,
            // since: "a/" < "a/aa" < "a/aa/aaa" < "a/ab/aba"
            // If problemMap keys are ["a/aa/aaa", "a/ab/aba"] and pathToMatch == "a/aa",
            // lower_bound(pathToMatch) will point to "a/aa/aaa", and the moment that
            // problemPath.startsWith(pathToMatch) == false, we know that we've looked
            // at everything that interest us.
            break;
        }
    }
    return SyncFileStatus::StatusNone;
}

/**
 * Whether this item should get an ERROR icon through the Socket API.
 *
 * The Socket API should only present serious, permanent errors to the user.
 * In particular SoftErrors should just retain their 'needs to be synced'
 * icon as the problem is most likely going to resolve itself quickly and
 * automatically.
 */
static inline bool hasErrorStatus(const SyncFileItem &item)
{
    const auto status = item._status;
    return item.instruction() == CSYNC_INSTRUCTION_ERROR || status == SyncFileItem::NormalError || status == SyncFileItem::FatalError
        || status == SyncFileItem::DetailError || status == SyncFileItem::BlacklistedError || item._hasBlacklistEntry;
}

static inline bool hasExcludedStatus(const SyncFileItem &item)
{
    const auto status = item._status;
    return item.instruction() == CSYNC_INSTRUCTION_IGNORE || status == SyncFileItem::FileIgnored || status == SyncFileItem::Conflict
        || status == SyncFileItem::Restoration;
}

SyncFileStatusTracker::SyncFileStatusTracker(SyncEngine *syncEngine)
    : _syncEngine(syncEngine)
    , _caseSensitivity(Utility::fsCaseSensitivity())
{
    connect(syncEngine, &SyncEngine::aboutToPropagate,
        this, &SyncFileStatusTracker::slotAboutToPropagate);
    connect(syncEngine, &SyncEngine::itemCompleted,
        this, &SyncFileStatusTracker::slotItemCompleted);
    connect(syncEngine, &SyncEngine::finished, this, &SyncFileStatusTracker::slotSyncFinished);
    connect(syncEngine, &SyncEngine::started, this, &SyncFileStatusTracker::slotSyncEngineRunningChanged);
    connect(syncEngine, &SyncEngine::finished, this, &SyncFileStatusTracker::slotSyncEngineRunningChanged);
}

SyncFileStatus SyncFileStatusTracker::fileStatus(const QString &relativePath)
{
    OC_ASSERT(!relativePath.endsWith(QLatin1Char('/')));

    if (relativePath.isEmpty()) {
        // This is the root sync folder, it doesn't have an entry in the database and won't be walked by csync, so resolve manually.
        return resolveSyncAndErrorStatus(QString(), NotShared);
    }

    const QString absolutePath = _syncEngine->localPath() + relativePath;

    if (!QFileInfo::exists(absolutePath)) {
        return SyncFileStatus(SyncFileStatus::StatusNone);
    }

    // The SyncEngine won't notify us at all for CSYNC_FILE_SILENTLY_EXCLUDED
    // and CSYNC_FILE_EXCLUDE_AND_REMOVE excludes. Even though it's possible
    // that the status of CSYNC_FILE_EXCLUDE_LIST excludes will change if the user
    // update the exclude list at runtime and doing it statically here removes
    // our ability to notify changes through the fileStatusChanged signal,
    // it's an acceptable compromize to treat all exclude types the same.
    // Update: This extra check shouldn't hurt even though silently excluded files
    // are now available via slotAddSilentlyExcluded().
    if (_syncEngine->isExcluded(_syncEngine->syncOptions()._vfs->underlyingFileName(absolutePath))) {
        return SyncFileStatus(SyncFileStatus::StatusExcluded);
    }

    if (_dirtyPaths.contains(relativePath))
        return SyncFileStatus::StatusSync;

    // First look it up in the database to know if it's shared
    SyncJournalFileRecord rec;
    if (_syncEngine->journal()->getFileRecord(relativePath, &rec) && rec.isValid()) {
        return resolveSyncAndErrorStatus(relativePath, rec._remotePerm.hasPermission(RemotePermissions::IsShared) ? Shared : NotShared);
    }

    // Must be a new file not yet in the database, check if it's syncing or has an error.
    return resolveSyncAndErrorStatus(relativePath, NotShared, PathUnknown);
}

void SyncFileStatusTracker::slotPathTouched(const QString &fileName)
{
    QString folderPath = _syncEngine->localPath();

    OC_ASSERT(fileName.startsWith(folderPath));
    QString localPath = fileName.mid(folderPath.size());
    _dirtyPaths.insert(localPath);

    emit fileStatusChanged(fileName, SyncFileStatus::StatusSync);
}

void SyncFileStatusTracker::slotAddSilentlyExcluded(const QString &folderPath)
{
    _syncProblems[folderPath] = SyncFileStatus::StatusExcluded;
    emit fileStatusChanged(getSystemDestination(folderPath), resolveSyncAndErrorStatus(folderPath, NotShared));
}

void SyncFileStatusTracker::incSyncCountAndEmitStatusChanged(const QString &relativePath, SharedFlag sharedFlag)
{
    // Will return 0 (and increase to 1) if the path wasn't in the map yet
    int count = _syncCount[relativePath]++;
    if (!count) {
        SyncFileStatus status = sharedFlag == UnknownShared
            ? fileStatus(relativePath)
            : resolveSyncAndErrorStatus(relativePath, sharedFlag);
        emit fileStatusChanged(getSystemDestination(relativePath), status);

        // We passed from OK to SYNC, increment the parent to keep it marked as
        // SYNC while we propagate ourselves and our own children.
        OC_ASSERT(!relativePath.endsWith(QLatin1Char('/')));
        int lastSlashIndex = relativePath.lastIndexOf(QLatin1Char('/'));
        if (lastSlashIndex != -1)
            incSyncCountAndEmitStatusChanged(relativePath.left(lastSlashIndex), UnknownShared);
        else if (!relativePath.isEmpty())
            incSyncCountAndEmitStatusChanged(QString(), UnknownShared);
    }
}

void SyncFileStatusTracker::decSyncCountAndEmitStatusChanged(const QString &relativePath, SharedFlag sharedFlag)
{
    int count = --_syncCount[relativePath];
    if (!count) {
        // Remove from the map, same as 0
        _syncCount.remove(relativePath);

        SyncFileStatus status = sharedFlag == UnknownShared
            ? fileStatus(relativePath)
            : resolveSyncAndErrorStatus(relativePath, sharedFlag);
        emit fileStatusChanged(getSystemDestination(relativePath), status);

        // We passed from SYNC to OK, decrement our parent.
        OC_ASSERT(!relativePath.endsWith(QLatin1Char('/')));
        int lastSlashIndex = relativePath.lastIndexOf(QLatin1Char('/'));
        if (lastSlashIndex != -1)
            decSyncCountAndEmitStatusChanged(relativePath.left(lastSlashIndex), UnknownShared);
        else if (!relativePath.isEmpty())
            decSyncCountAndEmitStatusChanged(QString(), UnknownShared);
    }
}

void SyncFileStatusTracker::slotAboutToPropagate(const SyncFileItemSet &items)
{
    OC_ASSERT(_syncCount.isEmpty());

    ProblemsMap oldProblems;
    std::swap(_syncProblems, oldProblems);

    for (const auto &item : qAsConst(items)) {
        qCDebug(lcStatusTracker) << "Investigating" << item->destination() << item->_status << item->instruction();
        _dirtyPaths.remove(item->destination());
        _dirtyPaths.remove(item->_originalFile);

        if (hasErrorStatus(*item)) {
            _syncProblems[item->destination()] = SyncFileStatus::StatusError;
            invalidateParentPaths(item->destination());
        } else if (hasExcludedStatus(*item)) {
            _syncProblems[item->destination()] = SyncFileStatus::StatusExcluded;
        }

        SharedFlag sharedFlag = item->_remotePerm.hasPermission(RemotePermissions::IsShared) ? Shared : NotShared;
        if (item->instruction() & ~(CSYNC_INSTRUCTION_NONE | CSYNC_INSTRUCTION_UPDATE_METADATA | CSYNC_INSTRUCTION_IGNORE | CSYNC_INSTRUCTION_ERROR)) {
            // Mark this path as syncing for instructions that will result in propagation.
            incSyncCountAndEmitStatusChanged(item->destination(), sharedFlag);
        } else {
            emit fileStatusChanged(getSystemDestination(item->destination()), resolveSyncAndErrorStatus(item->destination(), sharedFlag));
        }
    }

    // Some metadata status won't trigger files to be synced, make sure that we
    // push the OK status for dirty files that don't need to be propagated.
    // Swap into a copy since fileStatus() reads _dirtyPaths to determine the status
    QSet<QString> oldDirtyPaths;
    std::swap(_dirtyPaths, oldDirtyPaths);
    for (auto it = oldDirtyPaths.constBegin(); it != oldDirtyPaths.constEnd(); ++it)
        emit fileStatusChanged(getSystemDestination(*it), fileStatus(*it));

    // Make sure to push any status that might have been resolved indirectly since the last sync
    // (like an error file being deleted from disk)
    for (auto it = _syncProblems.begin(); it != _syncProblems.end(); ++it)
        oldProblems.erase(it->first);
    for (auto it = oldProblems.begin(); it != oldProblems.end(); ++it) {
        const QString &path = it->first;
        SyncFileStatus::SyncFileStatusTag severity = it->second;
        if (severity == SyncFileStatus::StatusError)
            invalidateParentPaths(path);
        emit fileStatusChanged(getSystemDestination(path), fileStatus(path));
    }
}

void SyncFileStatusTracker::slotItemCompleted(const SyncFileItemPtr &item)
{
    qCDebug(lcStatusTracker) << "Item completed" << item->destination() << item->_status << item->instruction();

    if (hasErrorStatus(*item)) {
        _syncProblems[item->destination()] = SyncFileStatus::StatusError;
        invalidateParentPaths(item->destination());
    } else if (hasExcludedStatus(*item)) {
        _syncProblems[item->destination()] = SyncFileStatus::StatusExcluded;
    } else {
        _syncProblems.erase(item->destination());
    }

    SharedFlag sharedFlag = item->_remotePerm.hasPermission(RemotePermissions::IsShared) ? Shared : NotShared;
    if (item->instruction() & ~(CSYNC_INSTRUCTION_NONE | CSYNC_INSTRUCTION_UPDATE_METADATA | CSYNC_INSTRUCTION_IGNORE | CSYNC_INSTRUCTION_ERROR)) {
        // decSyncCount calls *must* be symetric with incSyncCount calls in slotAboutToPropagate
        decSyncCountAndEmitStatusChanged(item->destination(), sharedFlag);
    } else {
        emit fileStatusChanged(getSystemDestination(item->destination()), resolveSyncAndErrorStatus(item->destination(), sharedFlag));
    }
}

void SyncFileStatusTracker::slotSyncFinished()
{
    // Clear the sync counts to reduce the impact of unsymetrical inc/dec calls (e.g. when directory job abort)
    QHash<QString, int> oldSyncCount;
    std::swap(_syncCount, oldSyncCount);
    for (auto it = oldSyncCount.begin(); it != oldSyncCount.end(); ++it)
        emit fileStatusChanged(getSystemDestination(it.key()), fileStatus(it.key()));
}

void SyncFileStatusTracker::slotSyncEngineRunningChanged()
{
    emit fileStatusChanged(getSystemDestination(QString()), resolveSyncAndErrorStatus(QString(), NotShared));
}

SyncFileStatus SyncFileStatusTracker::resolveSyncAndErrorStatus(const QString &relativePath, SharedFlag sharedFlag, PathKnownFlag isPathKnown)
{
    // If it's a new file and that we're not syncing it yet,
    // don't show any icon and wait for the filesystem watcher to trigger a sync.
    SyncFileStatus status(isPathKnown ? SyncFileStatus::StatusUpToDate : SyncFileStatus::StatusNone);
    if (_syncCount.value(relativePath)) {
        status.set(SyncFileStatus::StatusSync);
    } else {
        // After a sync finished, we need to show the users issues from that last sync like the activity list does.
        // Also used for parent directories showing a warning for an error child.
        SyncFileStatus::SyncFileStatusTag problemStatus = lookupProblem(relativePath, _syncProblems);
        if (problemStatus != SyncFileStatus::StatusNone)
            status.set(problemStatus);
    }

    OC_ASSERT_X(sharedFlag != UnknownShared,
        "The shared status needs to have been fetched from a SyncFileItem or the DB at this point.");
    if (sharedFlag == Shared)
        status.setShared(true);

    return status;
}

void SyncFileStatusTracker::invalidateParentPaths(const QString &path)
{
    QStringList splitPath = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i < splitPath.size(); ++i) {
        QString parentPath = QStringList(splitPath.mid(0, i)).join(QLatin1Char('/'));
        emit fileStatusChanged(getSystemDestination(parentPath), fileStatus(parentPath));
    }
}

QString SyncFileStatusTracker::getSystemDestination(const QString &relativePath)
{
    QString systemPath = _syncEngine->localPath() + relativePath;
    // SyncEngine::localPath() has a trailing slash, make sure to remove it if the
    // destination is empty.
    if (systemPath.endsWith(QLatin1Char('/'))) {
        systemPath.truncate(systemPath.length() - 1);
    }
    return systemPath;
}
}
