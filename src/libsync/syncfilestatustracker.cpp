/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncfilestatustracker.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/asserts.h"
#include "csync_exclude.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcStatusTracker, "nextcloud.sync.statustracker", QtInfoMsg)

static int pathCompare( const QString& lhs, const QString& rhs )
{
    // Should match Utility::fsCasePreserving, we want don't want to pay for the runtime check on every comparison.
    return lhs.compare(rhs,
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        Qt::CaseInsensitive
#else
        Qt::CaseSensitive
#endif
        );
}

static bool pathStartsWith( const QString& lhs, const QString& rhs )
{
    return lhs.startsWith(rhs,
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
        Qt::CaseInsensitive
#else
        Qt::CaseSensitive
#endif
        );
}

bool SyncFileStatusTracker::PathComparator::operator()( const QString& lhs, const QString& rhs ) const
{
    // This will make sure that the std::map is ordered and queried case-insensitively on macOS and Windows.
    return pathCompare(lhs, rhs) < 0;
}

SyncFileStatus::SyncFileStatusTag SyncFileStatusTracker::lookupProblem(const QString &pathToMatch, const SyncFileStatusTracker::ProblemsMap &problemMap)
{
    auto lower = problemMap.lower_bound(pathToMatch);
    for (auto it = lower; it != problemMap.cend(); ++it) {
        const QString &problemPath = it->first;
        SyncFileStatus::SyncFileStatusTag severity = it->second;

        if (pathCompare(problemPath, pathToMatch) == 0) {
            return severity;
        } else if (severity == SyncFileStatus::StatusError
            && pathStartsWith(problemPath, pathToMatch)
            && (pathToMatch.isEmpty() || problemPath.at(pathToMatch.size()) == '/')) {
            return SyncFileStatus::StatusWarning;
        } else if (!pathStartsWith(problemPath, pathToMatch)) {
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
    return item._instruction == CSYNC_INSTRUCTION_ERROR
        || status == SyncFileItem::NormalError
        || status == SyncFileItem::FatalError
        || status == SyncFileItem::DetailError
        || status == SyncFileItem::BlacklistedError
        || item._hasBlacklistEntry;
}

static inline bool hasExcludedStatus(const SyncFileItem &item)
{
    const auto status = item._status;
    return item._instruction == CSYNC_INSTRUCTION_IGNORE
        || status == SyncFileItem::FileIgnored
        || status == SyncFileItem::Conflict
        || status == SyncFileItem::Restoration
        || status == SyncFileItem::FileLocked;
}

SyncFileStatusTracker::SyncFileStatusTracker(SyncEngine *syncEngine)
    : _syncEngine(syncEngine)
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
    ASSERT(!relativePath.endsWith(QLatin1Char('/')));

    if (relativePath.isEmpty()) {
        // This is the root sync folder, it doesn't have an entry in the database and won't be walked by csync, so resolve manually.
        return resolveSyncAndErrorStatus(QString(), NotShared);
    }

    // The SyncEngine won't notify us at all for CSYNC_FILE_SILENTLY_EXCLUDED
    // and CSYNC_FILE_EXCLUDE_AND_REMOVE excludes. Even though it's possible
    // that the status of CSYNC_FILE_EXCLUDE_LIST excludes will change if the user
    // update the exclude list at runtime and doing it statically here removes
    // our ability to notify changes through the fileStatusChanged signal,
    // it's an acceptable compromise to treat all exclude types the same.
    // Update: This extra check shouldn't hurt even though silently excluded files
    // are now available via slotAddSilentlyExcluded().
    if (_syncEngine->excludedFiles().isExcluded(_syncEngine->localPath() + relativePath,
            _syncEngine->localPath(),
            _syncEngine->ignoreHiddenFiles())) {
        return SyncFileStatus::StatusExcluded;
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

    ASSERT(fileName.startsWith(folderPath));
    QString localPath = fileName.mid(folderPath.size());
    _dirtyPaths.insert(localPath);

    emit fileStatusChanged(fileName, SyncFileStatus::StatusSync);
}

void SyncFileStatusTracker::slotAddSilentlyExcluded(const QString &folderPath)
{
    _syncProblems[folderPath] = SyncFileStatus::StatusExcluded;
    _syncSilentExcludes[folderPath] = SyncFileStatus::StatusExcluded;
    emit fileStatusChanged(getSystemDestination(folderPath), resolveSyncAndErrorStatus(folderPath, NotShared));
}

void SyncFileStatusTracker::slotCheckAndRemoveSilentlyExcluded(const QString &folderPath)
{
    const auto foundIt = _syncSilentExcludes.find(folderPath);
    if (foundIt != _syncSilentExcludes.end()) {
        _syncSilentExcludes.erase(foundIt);
        emit fileStatusChanged(getSystemDestination(folderPath), SyncFileStatus::StatusUpToDate);
    }
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
        ASSERT(!relativePath.endsWith('/'));
        int lastSlashIndex = relativePath.lastIndexOf('/');
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
        ASSERT(!relativePath.endsWith('/'));
        int lastSlashIndex = relativePath.lastIndexOf('/');
        if (lastSlashIndex != -1)
            decSyncCountAndEmitStatusChanged(relativePath.left(lastSlashIndex), UnknownShared);
        else if (!relativePath.isEmpty())
            decSyncCountAndEmitStatusChanged(QString(), UnknownShared);
    }
}

void SyncFileStatusTracker::slotAboutToPropagate(SyncFileItemVector &items)
{
    ASSERT(_syncCount.isEmpty());

    ProblemsMap oldProblems;
    std::swap(_syncProblems, oldProblems);

    for (const auto &item : std::as_const(items)) {
        if (item->_instruction == CSyncEnums::CSYNC_INSTRUCTION_RENAME) {
            qCInfo(lcStatusTracker) << "Investigating" << item->destination() << item->_status << item->_instruction << item->_direction << item->_file << item->_originalFile << item->_renameTarget;
        } else {
            qCInfo(lcStatusTracker) << "Investigating" << item->destination() << item->_status << item->_instruction << item->_direction;
        }
        _dirtyPaths.remove(item->destination());

        if (hasErrorStatus(*item)) {
            _syncProblems[item->destination()] = SyncFileStatus::StatusError;
            _syncSilentExcludes.erase(item->destination());
            invalidateParentPaths(item->destination());
        } else if (hasExcludedStatus(*item)) {
            _syncProblems[item->destination()] = SyncFileStatus::StatusExcluded;
            _syncSilentExcludes.erase(item->destination());
        }

        SharedFlag sharedFlag = item->_remotePerm.hasPermission(RemotePermissions::IsShared) ? Shared : NotShared;
        if (item->_instruction != CSyncEnums::CSYNC_INSTRUCTION_REMOVE) {
            item->_discoveryResult.clear();
        }
        if (item->_instruction != CSYNC_INSTRUCTION_NONE
            && item->_instruction != CSYNC_INSTRUCTION_UPDATE_METADATA
            && item->_instruction != CSYNC_INSTRUCTION_IGNORE
            && item->_instruction != CSYNC_INSTRUCTION_ERROR) {
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
    for (const auto &oldDirtyPath : std::as_const(oldDirtyPaths))
        emit fileStatusChanged(getSystemDestination(oldDirtyPath), fileStatus(oldDirtyPath));

    // Make sure to push any status that might have been resolved indirectly since the last sync
    // (like an error file being deleted from disk)
    for (const auto &syncProblem : _syncProblems)
        oldProblems.erase(syncProblem.first);
    for (const auto &oldProblem : oldProblems) {
        const QString &path = oldProblem.first;
        SyncFileStatus::SyncFileStatusTag severity = oldProblem.second;
        if (severity == SyncFileStatus::StatusError)
            invalidateParentPaths(path);
        emit fileStatusChanged(getSystemDestination(path), fileStatus(path));
    }
}

void SyncFileStatusTracker::slotItemCompleted(const SyncFileItemPtr &item)
{
    qCDebug(lcStatusTracker) << "Item completed" << item->destination() << item->_status << item->_instruction;

    if (hasErrorStatus(*item)) {
        _syncProblems[item->destination()] = SyncFileStatus::StatusError;
        invalidateParentPaths(item->destination());
    } else if (hasExcludedStatus(*item)) {
        _syncProblems[item->destination()] = SyncFileStatus::StatusExcluded;
    } else {
        _syncProblems.erase(item->destination());
    }
    _syncSilentExcludes.erase(item->destination());

    SharedFlag sharedFlag = item->_remotePerm.hasPermission(RemotePermissions::IsShared) ? Shared : NotShared;
    if (item->_instruction != CSYNC_INSTRUCTION_NONE
        && item->_instruction != CSYNC_INSTRUCTION_UPDATE_METADATA
        && item->_instruction != CSYNC_INSTRUCTION_IGNORE
        && item->_instruction != CSYNC_INSTRUCTION_ERROR) {
        // decSyncCount calls *must* be symmetric with incSyncCount calls in slotAboutToPropagate
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
    for (auto it = oldSyncCount.begin(); it != oldSyncCount.end(); ++it) {
        // Don't announce folders, fileStatus expect only paths without '/', otherwise it asserts
        if (it.key().endsWith('/')) {
            continue;
        }

        emit fileStatusChanged(getSystemDestination(it.key()), fileStatus(it.key()));
    }
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

    ASSERT(sharedFlag != UnknownShared,
        "The shared status needs to have been fetched from a SyncFileItem or the DB at this point.");
    if (sharedFlag == Shared)
        status.setShared(true);

    return status;
}

void SyncFileStatusTracker::invalidateParentPaths(const QString &path)
{
    QStringList splitPath = path.split('/', Qt::SkipEmptyParts);
    for (int i = 0; i < splitPath.size(); ++i) {
        QString parentPath = QStringList(splitPath.mid(0, i)).join(QLatin1String("/"));
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
