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

#include "syncfilestatustracker.h"
#include "filesystem.h"
#include "syncengine.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "utility.h"

#include <QDir>
#include <QFileInfo>

namespace OCC {

/**
 * Whether this item should get an ERROR icon through the Socket API.
 *
 * The Socket API should only present serious, permanent errors to the user.
 * In particular SoftErrors should just retain their 'needs to be synced'
 * icon as the problem is most likely going to resolve itself quickly and
 * automatically.
 */
static bool showErrorInSocketApi(const SyncFileItem& item)
{
    const auto status = item._status;
    return status == SyncFileItem::NormalError
        || status == SyncFileItem::FatalError;
}

static void addErroredSyncItemPathsToList(const SyncFileItemVector& items, QSet<QString>* set) {
    foreach (const SyncFileItemPtr &item, items) {
        if (showErrorInSocketApi(*item)) {
            set->insert(item->_file);
        }
    }
}

SyncFileStatusTracker::SyncFileStatusTracker(SyncEngine *syncEngine)
    : _syncEngine(syncEngine)
{
    connect(syncEngine, SIGNAL(treeWalkResult(const SyncFileItemVector&)),
              this, SLOT(slotThreadTreeWalkResult(const SyncFileItemVector&)));
    connect(syncEngine, SIGNAL(aboutToPropagate(SyncFileItemVector&)),
              this, SLOT(slotAboutToPropagate(SyncFileItemVector&)));
    connect(syncEngine, SIGNAL(finished(bool)), SLOT(slotSyncFinished()));
    connect(syncEngine, SIGNAL(itemCompleted(const SyncFileItem&, const PropagatorJob&)),
            this, SLOT(slotItemCompleted(const SyncFileItem&)));
    connect(syncEngine, SIGNAL(syncItemDiscovered(const SyncFileItem&)),
            this, SLOT(slotItemDiscovered(const SyncFileItem&)));
}

bool SyncFileStatusTracker::estimateState(QString fn, csync_ftw_type_e t, SyncFileStatus* s)
{
    if (t == CSYNC_FTW_TYPE_DIR) {
        if (Utility::doesSetContainPrefix(_stateLastSyncItemsWithError, fn)) {
            qDebug() << Q_FUNC_INFO << "Folder has error" << fn;
            s->set(SyncFileStatus::StatusError);
            return true;
        }
        // If sync is running, check _syncedItems, possibly give it StatusSync
        if (_syncEngine->isSyncRunning()) {
            if (_syncEngine->estimateState(fn, t, s)) {
                return true;
            }
        }
        return false;
    } else if ( t== CSYNC_FTW_TYPE_FILE) {
        // check if errorList has the directory/file
        if (Utility::doesSetContainPrefix(_stateLastSyncItemsWithError, fn)) {
            s->set(SyncFileStatus::StatusError);
            return true;
        }
        // If sync running: _syncedItems -> SyncingState
        if (_syncEngine->isSyncRunning()) {
            if (_syncEngine->estimateState(fn, t, s)) {
                return true;
            }
        }
    }
    return false;
}


/**
 * Get status about a single file.
 */
SyncFileStatus SyncFileStatusTracker::fileStatus(const QString& systemFileName)
{
    QString file = _syncEngine->localPath();
    QString fileName = systemFileName.normalized(QString::NormalizationForm_C);
    QString fileNameSlash = fileName;

    if(fileName != QLatin1String("/") && !fileName.isEmpty()) {
        file += fileName;
    }

    if( fileName.endsWith(QLatin1Char('/')) ) {
        fileName.truncate(fileName.length()-1);
        qDebug() << "Removed trailing slash: " << fileName;
    } else {
        fileNameSlash += QLatin1Char('/');
    }

    const QFileInfo fi(file);
    if( !FileSystem::fileExists(file, fi) ) {
        qDebug() << "OO File " << file << " is not existing";
        return SyncFileStatus(SyncFileStatus::StatusError);
    }

    // file is ignored?
    // Qt considers .lnk files symlinks on Windows so we need to work
    // around that here.
    if( fi.isSymLink()
#ifdef Q_OS_WIN
            && fi.suffix() != "lnk"
#endif
            ) {
        return SyncFileStatus(SyncFileStatus::StatusIgnore);
    }

    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    // Is it excluded?
    if( _syncEngine->excludedFiles().isExcluded(file, _syncEngine->localPath(), _syncEngine->ignoreHiddenFiles()) ) {
        return SyncFileStatus(SyncFileStatus::StatusIgnore);
    }

    // Error if it is in the selective sync blacklist
    foreach(const auto &s, _syncEngine->journal()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList)) {
        if (fileNameSlash.startsWith(s)) {
            return SyncFileStatus(SyncFileStatus::StatusError);
        }
    }

    SyncFileStatus status(SyncFileStatus::StatusNone);
    SyncJournalFileRecord rec = _syncEngine->journal()->getFileRecord(fileName );

    if (estimateState(fileName, type, &status)) {
        qDebug() << "Folder estimated status for" << fileName << "to" << status.toSocketAPIString();
    } else if (fileName == "") {
        // sync folder itself
        // FIXME: The new parent folder logic should take over this, treating the root the same as any folder.
    } else if (type == CSYNC_FTW_TYPE_DIR) {
        if (rec.isValid()) {
            status.set(SyncFileStatus::StatusUpToDate);
        } else {
            qDebug() << "Could not determine state for folder" << fileName << "will set StatusSync";
            status.set(SyncFileStatus::StatusSync);
        }
    } else if (type == CSYNC_FTW_TYPE_FILE) {
        if (rec.isValid()) {
            if( FileSystem::getModTime(fi.absoluteFilePath()) == Utility::qDateTimeToTime_t(rec._modtime) ) {
                status.set(SyncFileStatus::StatusUpToDate);
            } else {
                if (rec._remotePerm.isNull() || rec._remotePerm.contains("W") ) {
                    status.set(SyncFileStatus::StatusSync);
                } else {
                    status.set(SyncFileStatus::StatusError);
                }
            }
        } else {
            qDebug() << "Could not determine state for file" << fileName << "will set StatusSync";
            status.set(SyncFileStatus::StatusSync);
        }
    }

    if (rec.isValid() && rec._remotePerm.contains("S"))
        status.setSharedWithMe(true);

    // FIXME: Wrong, but will go away
    if (status.tag() == SyncFileStatus::StatusSync) {
        // check the parent folder if it is shared and if it is allowed to create a file/dir within
        QDir d( fi.path() );
        auto parentPath = d.path();
        auto dirRec = _syncEngine->journal()->getFileRecord(parentPath);
        bool isDir = type == CSYNC_FTW_TYPE_DIR;
        while( !d.isRoot() && !(d.exists() && dirRec.isValid()) ) {
            d.cdUp(); // returns true if the dir exists.

            parentPath = d.path();
            // cut the folder path
            dirRec = _syncEngine->journal()->getFileRecord(parentPath);

            isDir = true;
        }
        if( dirRec.isValid() && !dirRec._remotePerm.isNull()) {
            if( (isDir && !dirRec._remotePerm.contains("K"))
                    || (!isDir && !dirRec._remotePerm.contains("C")) ) {
                status.set(SyncFileStatus::StatusError);
            }
        }
    }
    return status;
}

void SyncFileStatusTracker::slotThreadTreeWalkResult(const SyncFileItemVector& items)
{
    addErroredSyncItemPathsToList(items, &_stateLastSyncItemsWithErrorNew);
}

void SyncFileStatusTracker::slotAboutToPropagate(SyncFileItemVector& items)
{
    addErroredSyncItemPathsToList(items, &_stateLastSyncItemsWithErrorNew);
}

void SyncFileStatusTracker::slotSyncFinished()
{
    _stateLastSyncItemsWithError = _stateLastSyncItemsWithErrorNew;
    _stateLastSyncItemsWithErrorNew.clear();
}

void SyncFileStatusTracker::slotItemCompleted(const SyncFileItem &item)
{
    if (showErrorInSocketApi(item)) {
        _stateLastSyncItemsWithErrorNew.insert(item._file);
    }

    QString systemFileName = _syncEngine->localPath() + item.destination();

    // the trailing slash for directories must be appended as the filenames coming in
    // from the plugins have that too. Otherwise the matching entry item is not found
    // in the plugin.
    if( item._type == SyncFileItem::Type::Directory ) {
        systemFileName += QLatin1Char('/');
    }

    auto status = fileStatus(item.destination());
    emit fileStatusChanged(systemFileName, status);
}

void SyncFileStatusTracker::slotItemDiscovered(const SyncFileItem &item)
{
    QString systemFileName = _syncEngine->localPath() + item.destination();

    // the trailing slash for directories must be appended as the filenames coming in
    // from the plugins have that too. Otherwise the matching entry item is not found
    // in the plugin.
    if( item._type == SyncFileItem::Type::Directory ) {
        systemFileName += QLatin1Char('/');
    }

    emit fileStatusChanged(systemFileName, SyncFileStatus(SyncFileStatus::StatusSync));
}

}
