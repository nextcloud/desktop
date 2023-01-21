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
    Q_GADGET
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
    Q_ENUM(Status);

    SyncResult();
    void reset();

    void appendErrorString(const QString &);
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QStringList errorStrings() const;
    void clearErrors();

    void setStatus(Status);
    [[nodiscard]] Status status() const;
    [[nodiscard]] QString statusString() const;
    [[nodiscard]] QDateTime syncTime() const;
    void setFolder(const QString &folder);
    [[nodiscard]] QString folder() const;

    [[nodiscard]] bool foundFilesNotSynced() const { return _foundFilesNotSynced; }
    [[nodiscard]] bool folderStructureWasChanged() const { return _folderStructureWasChanged; }

    [[nodiscard]] int numNewItems() const { return _numNewItems; }
    [[nodiscard]] int numRemovedItems() const { return _numRemovedItems; }
    [[nodiscard]] int numUpdatedItems() const { return _numUpdatedItems; }
    [[nodiscard]] int numRenamedItems() const { return _numRenamedItems; }
    [[nodiscard]] int numNewConflictItems() const { return _numNewConflictItems; }
    [[nodiscard]] int numOldConflictItems() const { return _numOldConflictItems; }
    void setNumOldConflictItems(int n) { _numOldConflictItems = n; }
    [[nodiscard]] int numErrorItems() const { return _numErrorItems; }
    [[nodiscard]] bool hasUnresolvedConflicts() const { return _numNewConflictItems + _numOldConflictItems > 0; }

    [[nodiscard]] int numLockedItems() const { return _numLockedItems; }
    [[nodiscard]] bool hasLockedFiles() const { return _numLockedItems > 0; }

    [[nodiscard]] const SyncFileItemPtr &firstItemNew() const { return _firstItemNew; }
    [[nodiscard]] const SyncFileItemPtr &firstItemDeleted() const { return _firstItemDeleted; }
    [[nodiscard]] const SyncFileItemPtr &firstItemUpdated() const { return _firstItemUpdated; }
    [[nodiscard]] const SyncFileItemPtr &firstItemRenamed() const { return _firstItemRenamed; }
    [[nodiscard]] const SyncFileItemPtr &firstNewConflictItem() const { return _firstNewConflictItem; }
    [[nodiscard]] const SyncFileItemPtr &firstItemError() const { return _firstItemError; }
    [[nodiscard]] const SyncFileItemPtr &firstItemLocked() const { return _firstItemLocked; }

    void processCompletedItem(const SyncFileItemPtr &item);

private:
    Status _status = Undefined;
    SyncFileItemVector _syncItems;
    QDateTime _syncTime;
    QString _folder;
    /**
     * when the sync tool support this...
     */
    QStringList _errors;
    bool _foundFilesNotSynced = false;
    bool _folderStructureWasChanged = false;

    // count new, removed and updated items
    int _numNewItems = 0;
    int _numRemovedItems = 0;
    int _numUpdatedItems = 0;
    int _numRenamedItems = 0;
    int _numNewConflictItems = 0;
    int _numOldConflictItems = 0;
    int _numErrorItems = 0;
    int _numLockedItems = 0;

    SyncFileItemPtr _firstItemNew;
    SyncFileItemPtr _firstItemDeleted;
    SyncFileItemPtr _firstItemUpdated;
    SyncFileItemPtr _firstItemRenamed;
    SyncFileItemPtr _firstNewConflictItem;
    SyncFileItemPtr _firstItemError;
    SyncFileItemPtr _firstItemLocked;
};
}

#endif
