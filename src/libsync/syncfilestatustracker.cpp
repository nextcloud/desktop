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

static SyncFileStatus::SyncFileStatusTag lookupProblem(const QString &pathToMatch, const std::set<Problem> &set)
{
    for (auto it = set.cbegin(); it != set.cend(); ++it) {
        qDebug() << Q_FUNC_INFO << pathToMatch << it->severity << it->path;
        auto problemPath = it->path;
        if (problemPath == pathToMatch)
            return it->severity;
        else if (it->severity == SyncFileStatus::StatusError && problemPath.startsWith(pathToMatch))
            return SyncFileStatus::StatusWarning;
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


SyncFileStatus SyncFileStatusTracker::fileStatus(const QString& systemFileName)
{
    QString fileName = systemFileName.normalized(QString::NormalizationForm_C);
    if( fileName.endsWith(QLatin1Char('/')) ) {
        fileName.truncate(fileName.length()-1);
        qDebug() << "Removed trailing slash: " << fileName;
    }

    SyncFileItem* item = _syncEngine->findSyncItem(fileName);
    if (item)
        return fileStatus(*item);

    // If we're not currently syncing that file, look it up in the database to know if it's shared
    SyncJournalFileRecord rec = _syncEngine->journal()->getFileRecord(fileName);
    if (rec.isValid())
        return fileStatus(rec.toSyncFileItem());

    // Must be a new file, wait for the filesystem watcher to trigger a sync
    return SyncFileStatus();
}

void SyncFileStatusTracker::slotAboutToPropagate(SyncFileItemVector& items)
{
    std::set<Problem> oldProblems;
    std::swap(_syncProblems, oldProblems);

    foreach (const SyncFileItemPtr &item, items) {
        qDebug() << Q_FUNC_INFO << "Investigating" << item->destination() << item->_status;

        if (showErrorInSocketApi(*item))
            _syncProblems.insert({item->_file, SyncFileStatus::StatusError});
        else if (showWarningInSocketApi(*item))
            _syncProblems.insert({item->_file, SyncFileStatus::StatusWarning});

        QString systemFileName = _syncEngine->localPath() + item->destination();
        // the trailing slash for directories must be appended as the filenames coming in
        // from the plugins have that too. Otherwise the matching entry item is not found
        // in the plugin.
        if( item->_type == SyncFileItem::Type::Directory )
            systemFileName += QLatin1Char('/');
        emit fileStatusChanged(systemFileName, fileStatus(*item));
    }

    // Make sure to push any status that might have been resolved indirectly since the last sync
    // (like an error file being deleted from disk)
    for (auto it = _syncProblems.begin(); it != _syncProblems.end(); ++it)
        oldProblems.erase(*it);
    for (auto it = oldProblems.begin(); it != oldProblems.end(); ++it) {
        if (it->severity == SyncFileStatus::StatusError)
            invalidateParentPaths(it->path);
        emit fileStatusChanged(_syncEngine->localPath() + it->path, fileStatus(it->path));
    }
}

void SyncFileStatusTracker::slotItemCompleted(const SyncFileItem &item)
{
    qDebug() << Q_FUNC_INFO << item.destination() << item._status;

    if (showErrorInSocketApi(item)) {
        _syncProblems.insert({item._file, SyncFileStatus::StatusError});
        invalidateParentPaths(item.destination());
    } else if (showWarningInSocketApi(item)) {
        _syncProblems.insert({item._file, SyncFileStatus::StatusWarning});
    } else {
        // There is currently no situation where an error status set during discovery/update is fixed by propagation.
        Q_ASSERT(_syncProblems.find(item._file) == _syncProblems.end());
    }

    QString systemFileName = _syncEngine->localPath() + item.destination();
    // the trailing slash for directories must be appended as the filenames coming in
    // from the plugins have that too. Otherwise the matching entry item is not found
    // in the plugin.
    if( item._type == SyncFileItem::Type::Directory ) {
        systemFileName += QLatin1Char('/');
    }
    emit fileStatusChanged(systemFileName, fileStatus(item));
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
        QString parentPath = splitPath.mid(0, i).join('/');
        emit fileStatusChanged(_syncEngine->localPath() + parentPath, fileStatus(parentPath));
    }
}

}
