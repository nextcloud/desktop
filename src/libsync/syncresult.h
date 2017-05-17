/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef MIRALL_SYNCRESULT_H
#define MIRALL_SYNCRESULT_H

#include <QStringList>
#include <QHash>
#include <QDateTime>

#include "owncloudlib.h"
#include "syncfileitem.h"

namespace OCC {

/**
 * @brief The SyncResult class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncResult
{
public:
    enum Status {
        Undefined,
        NotYetStarted,
        SyncPrepare,
        SyncRunning,
        SyncAbortRequested,
        Success,
        Problem,
        Error,
        SetupError,
        Paused
    };

    SyncResult();
    void reset();

    void appendErrorString(const QString &);
    QString errorString() const;
    QStringList errorStrings() const;
    void clearErrors();

    void setStatus(Status);
    Status status() const;
    QString statusString() const;
    QDateTime syncTime() const;
    void setFolder(const QString &folder);
    QString folder() const;

    bool foundFilesNotSynced() const { return _foundFilesNotSynced; }
    bool folderStructureWasChanged() const { return _folderStructureWasChanged; }

    int numNewItems() const { return _numNewItems; }
    int numRemovedItems() const { return _numRemovedItems; }
    int numUpdatedItems() const { return _numUpdatedItems; }
    int numRenamedItems() const { return _numRenamedItems; }
    int numConflictItems() const { return _numConflictItems; }
    int numErrorItems() const { return _numErrorItems; }

    const SyncFileItemPtr &firstItemNew() const { return _firstItemNew; }
    const SyncFileItemPtr &firstItemDeleted() const { return _firstItemDeleted; }
    const SyncFileItemPtr &firstItemUpdated() const { return _firstItemUpdated; }
    const SyncFileItemPtr &firstItemRenamed() const { return _firstItemRenamed; }
    const SyncFileItemPtr &firstConflictItem() const { return _firstConflictItem; }
    const SyncFileItemPtr &firstItemError() const { return _firstItemError; }

    void processCompletedItem(const SyncFileItemPtr &item);

private:
    Status _status;
    SyncFileItemVector _syncItems;
    QDateTime _syncTime;
    QString _folder;
    /**
     * when the sync tool support this...
     */
    QStringList _errors;
    bool _foundFilesNotSynced;
    bool _folderStructureWasChanged;

    // count new, removed and updated items
    int _numNewItems;
    int _numRemovedItems;
    int _numUpdatedItems;
    int _numRenamedItems;
    int _numConflictItems;
    int _numErrorItems;

    SyncFileItemPtr _firstItemNew;
    SyncFileItemPtr _firstItemDeleted;
    SyncFileItemPtr _firstItemUpdated;
    SyncFileItemPtr _firstItemRenamed;
    SyncFileItemPtr _firstConflictItem;
    SyncFileItemPtr _firstItemError;
};
}

#endif
