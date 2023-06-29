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

#include "common/utility.h"
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
    // the order of the values markes their importance
    // higher values take prcedence when computing the
    // overall status
    enum Status {
        Undefined,
        Success,
        NotYetStarted,
        SyncPrepare,
        SyncRunning,
        SyncAbortRequested,
        Paused,
        Offline,
        Problem,
        Error,
        SetupError,
    };
    Q_ENUM(Status);

    SyncResult() = default;
    explicit SyncResult(Status status);
    void reset();

    void appendErrorString(const QString &);
    QString errorString() const;
    QStringList errorStrings() const;
    void clearErrors();

    void setStatus(Status);
    Status status() const;
    QDateTime syncTime() const;

    bool foundFilesNotSynced() const { return _foundFilesNotSynced; }
    bool folderStructureWasChanged() const { return _folderStructureWasChanged; }

    int numNewItems() const { return _numNewItems; }
    int numRemovedItems() const { return _numRemovedItems; }
    int numUpdatedItems() const { return _numUpdatedItems; }
    int numRenamedItems() const { return _numRenamedItems; }
    int numNewConflictItems() const { return _numNewConflictItems; }
    int numOldConflictItems() const { return _numOldConflictItems; }
    void setNumOldConflictItems(int n) { _numOldConflictItems = n; }
    int numErrorItems() const { return _numErrorItems; }
    bool hasUnresolvedConflicts() const { return _numNewConflictItems + _numOldConflictItems > 0; }

    const SyncFileItemPtr &firstItemNew() const { return _firstItemNew; }
    const SyncFileItemPtr &firstItemDeleted() const { return _firstItemDeleted; }
    const SyncFileItemPtr &firstItemUpdated() const { return _firstItemUpdated; }
    const SyncFileItemPtr &firstItemRenamed() const { return _firstItemRenamed; }
    const SyncFileItemPtr &firstNewConflictItem() const { return _firstNewConflictItem; }
    const SyncFileItemPtr &firstItemError() const { return _firstItemError; }

    void processCompletedItem(const SyncFileItemPtr &item);

    int numBlacklistErrors() const;

private:
    Status _status = Undefined;
    SyncFileItemSet _syncItems;
    QDateTime _syncTime;
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
    int _numBlacklistErrors = 0;
    int _numNewConflictItems = 0;
    int _numOldConflictItems = 0;
    int _numErrorItems = 0;

    SyncFileItemPtr _firstItemNew;
    SyncFileItemPtr _firstItemDeleted;
    SyncFileItemPtr _firstItemUpdated;
    SyncFileItemPtr _firstItemRenamed;
    SyncFileItemPtr _firstNewConflictItem;
    SyncFileItemPtr _firstItemError;

    friend class TrayOverallStatusResult;
};

template <>
OWNCLOUDSYNC_EXPORT QString Utility::enumToDisplayName(SyncResult::Status status);
}

#endif
