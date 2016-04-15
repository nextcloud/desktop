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

#include "syncfilestatustracker.h"
#include "syncengine.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"

namespace OCC {

static SyncFileStatus::SyncFileStatusTag lookupProblem(const QString &pathToMatch, const std::map<QString, SyncFileStatus::SyncFileStatusTag> &problemMap)
{
    auto lower = problemMap.lower_bound(pathToMatch);
    for (auto it = lower; it != problemMap.cend(); ++it) {
        const QString &problemPath = it->first;
        SyncFileStatus::SyncFileStatusTag severity = it->second;
        // qDebug() << Q_FUNC_INFO << pathToMatch << severity << problemPath;
        if (problemPath == pathToMatch) {
            return severity;
        } else if (severity == SyncFileStatus::StatusError && problemPath.startsWith(pathToMatch) && problemPath.at(pathToMatch.size()) == '/') {
            Q_ASSERT(!pathToMatch.endsWith('/'));
            return SyncFileStatus::StatusWarning;
        } else if (!problemPath.startsWith(pathToMatch)) {
            // Starting at lower_bound we get the first path that is not smaller,
            // since: "/a/" < "/a/aa" < "/a/aa/aaa" < "/a/ab/aba"
            // If problemMap keys are ["/a/aa/aaa", "/a/ab/aba"] and pathToMatch == "/a/aa",
            // lower_bound(pathToMatch) will point to "/a/aa/aaa", and the moment that
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
static inline bool showErrorInSocketApi(const SyncFileItem& item)
{
    const auto status = item._status;
    return status == SyncFileItem::NormalError
        || status == SyncFileItem::FatalError
        || item._hasBlacklistEntry;
}

static inline bool showWarningInSocketApi(const SyncFileItem& item)
{
    const auto status = item._status;
    return status == SyncFileItem::FileIgnored
        || status == SyncFileItem::Conflict
        || status == SyncFileItem::Restoration;
}

SyncFileStatusTracker::SyncFileStatusTracker(SyncEngine *syncEngine)
    : _syncEngine(syncEngine)
{
    connect(syncEngine, SIGNAL(aboutToPropagate(SyncFileItemVector&)),
              this, SLOT(slotAboutToPropagate(SyncFileItemVector&)));
    connect(syncEngine, SIGNAL(itemCompleted(const SyncFileItem&, const PropagatorJob&)),
            this, SLOT(slotItemCompleted(const SyncFileItem&)));
}

SyncFileStatus SyncFileStatusTracker::rootStatus()
{
    /* Possible values for the status:
    enum SyncFileStatusTag {
        StatusNone,
        StatusSync,
        StatusWarning,
        StatusUpToDate,
        StatusError,
    };
    */
    SyncFileStatus status =  SyncFileStatus::StatusUpToDate;

    if( !_syncEngine ) return SyncFileStatus::StatusNone;

    if( _syncEngine->isSyncRunning() ) {
        status = SyncFileStatus::StatusSync;
    } else {
        // sync is not running. Check dirty list and _syncProblems
        int errs = 0, warns = 0;
        for (auto it = _syncProblems.begin(); it != _syncProblems.end(); ++it) {
            if( it->second == SyncFileStatus::StatusError ) {
                errs ++;
                break; // stop if an error found at all.
            } else {
                warns ++;
            }
        }
        if( errs ) {
            status = SyncFileStatus::StatusError;
        } else if( warns ) {
            status = SyncFileStatus::StatusWarning;
        }
    }
    return status;

}

SyncFileStatus SyncFileStatusTracker::fileStatus(const QString& systemFileName)
{
    QString fileName = systemFileName.normalized(QString::NormalizationForm_C);
    if( fileName.endsWith(QLatin1Char('/')) ) {
        fileName.truncate(fileName.length()-1);
        qDebug() << "Removed trailing slash: " << fileName;
    }

    if( fileName.isEmpty() ) {
        // this is the root sync folder.
        return rootStatus();

    }
    // The SyncEngine won't notify us at all for CSYNC_FILE_SILENTLY_EXCLUDED
    // and CSYNC_FILE_EXCLUDE_AND_REMOVE excludes. Even though it's possible
    // that the status of CSYNC_FILE_EXCLUDE_LIST excludes will change if the user
    // update the exclude list at runtime and doing it statically here removes
    // our ability to notify changes through the fileStatusChanged signal,
    // it's an acceptable compromize to treat all exclude types the same.
    if( _syncEngine->excludedFiles().isExcluded(_syncEngine->localPath() + fileName, _syncEngine->localPath(), _syncEngine->ignoreHiddenFiles()) )
        return SyncFileStatus(SyncFileStatus::StatusWarning);

    SyncFileItem* item = _syncEngine->findSyncItem(fileName);
    if (item) {
        return fileStatus(*item);
    }

    // If we're not currently syncing that file, look it up in the database to know if it's shared
    SyncJournalFileRecord rec = _syncEngine->journal()->getFileRecord(fileName);
    if (rec.isValid())
        return fileStatus(rec.toSyncFileItem());

    // Must be a new file, wait for the filesystem watcher to trigger a sync
    return SyncFileStatus();
}

void SyncFileStatusTracker::slotAboutToPropagate(SyncFileItemVector& items)
{
    std::map<QString, SyncFileStatus::SyncFileStatusTag> oldProblems;
    std::swap(_syncProblems, oldProblems);

    foreach (const SyncFileItemPtr &item, items) {
        // qDebug() << Q_FUNC_INFO << "Investigating" << item->destination() << item->_status;

        if (showErrorInSocketApi(*item))
            _syncProblems[item->_file] = SyncFileStatus::StatusError;
        else if (showWarningInSocketApi(*item))
            _syncProblems[item->_file] = SyncFileStatus::StatusWarning;

        emit fileStatusChanged(getSystemDestination(*item), fileStatus(*item));
    }

    // Make sure to push any status that might have been resolved indirectly since the last sync
    // (like an error file being deleted from disk)
    for (auto it = _syncProblems.begin(); it != _syncProblems.end(); ++it)
        oldProblems.erase(it->first);
    for (auto it = oldProblems.begin(); it != oldProblems.end(); ++it) {
        const QString &path = it->first;
        SyncFileStatus::SyncFileStatusTag severity = it->second;
        if (severity == SyncFileStatus::StatusError)
            invalidateParentPaths(path);
        emit fileStatusChanged(_syncEngine->localPath() + path, fileStatus(path));
    }
}

void SyncFileStatusTracker::slotItemCompleted(const SyncFileItem &item)
{
    // qDebug() << Q_FUNC_INFO << item.destination() << item._status;

    if (showErrorInSocketApi(item)) {
        _syncProblems[item._file] = SyncFileStatus::StatusError;
        invalidateParentPaths(item.destination());
    } else if (showWarningInSocketApi(item)) {
        _syncProblems[item._file] = SyncFileStatus::StatusWarning;
    } else {
        // There is currently no situation where an error status set during discovery/update is fixed by propagation.
        Q_ASSERT(_syncProblems.find(item._file) == _syncProblems.end());
    }

    emit fileStatusChanged(getSystemDestination(item), fileStatus(item));
}

SyncFileStatus SyncFileStatusTracker::fileStatus(const SyncFileItem& item)
{
    // Hack to know if the item was taken from the sync engine (Sync), or from the database (UpToDate)
    bool waitingForPropagation = item._direction != SyncFileItem::None && item._status == SyncFileItem::NoStatus;

    SyncFileStatus status(SyncFileStatus::StatusUpToDate);
    if (waitingForPropagation) {
        status.set(SyncFileStatus::StatusSync);
    } else if (showErrorInSocketApi(item)) {
        status.set(SyncFileStatus::StatusError);
    } else if (showWarningInSocketApi(item)) {
        status.set(SyncFileStatus::StatusWarning);
    } else {
        // After a sync finished, we need to show the users issues from that last sync like the activity list does.
        // Also used for parent directories showing a warning for an error child.
        SyncFileStatus::SyncFileStatusTag problemStatus = lookupProblem(item.destination(), _syncProblems);
        if (problemStatus != SyncFileStatus::StatusNone)
            status.set(problemStatus);
    }

    if (item._remotePerm.contains("S"))
        status.setSharedWithMe(true);

    return status;
}

void SyncFileStatusTracker::invalidateParentPaths(const QString& path)
{
    QStringList splitPath = path.split('/', QString::SkipEmptyParts);
    for (int i = 0; i < splitPath.size(); ++i) {
        QString parentPath = QStringList(splitPath.mid(0, i)).join(QLatin1String("/"));
        emit fileStatusChanged(_syncEngine->localPath() + parentPath, fileStatus(parentPath));
    }
}

QString SyncFileStatusTracker::getSystemDestination(const SyncFileItem& item)
{
    QString systemFileName = _syncEngine->localPath() + item.destination();
    // the trailing slash for directories must be appended as the filenames coming in
    // from the plugins have that too. Otherwise the matching entry item is not found
    // in the plugin.
    if( item._type == SyncFileItem::Type::Directory ) {
        systemFileName += QLatin1Char('/');
    }
    return systemFileName;
}

}
