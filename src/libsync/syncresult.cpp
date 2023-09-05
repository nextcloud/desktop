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

#include "syncresult.h"
#include "progressdispatcher.h"
#include "theme.h"

namespace OCC {

SyncResult::SyncResult(Status status)
    : _status(status)
{
}


SyncResult::Status SyncResult::status() const
{
    return _status;
}

void SyncResult::reset()
{
    *this = SyncResult();
}

template <>
QString Utility::enumToDisplayName(SyncResult::Status status)
{
    switch (status) {
    case SyncResult::Status::Undefined:
        return QStringLiteral("Undefined");
    case SyncResult::Status::NotYetStarted:
        return QStringLiteral("Awaiting sync");
    case SyncResult::Status::SyncRunning:
        return QStringLiteral("Sync running");
    case SyncResult::Status::Success:
        return QStringLiteral("Success");
    case SyncResult::Status::Error:
        return QStringLiteral("Error");
    case SyncResult::Status::SetupError:
        return QStringLiteral("Setup error");
    case SyncResult::Status::SyncPrepare:
        return QStringLiteral("Preparing to sync");
    case SyncResult::Status::Problem:
        return QStringLiteral("Success, some files were ignored.");
    case SyncResult::Status::SyncAbortRequested:
        return QStringLiteral("Aborting sync");
    case SyncResult::Status::Paused:
        return QStringLiteral("Sync paused");
    case SyncResult::Status::Offline:
        return QStringLiteral("Offline");
    }
    Q_UNREACHABLE();
}

void SyncResult::setStatus(Status stat)
{
    _status = stat;
    _syncTime = QDateTime::currentDateTimeUtc();
}

QDateTime SyncResult::syncTime() const
{
    return _syncTime;
}

QStringList SyncResult::errorStrings() const
{
    return _errors;
}

void SyncResult::appendErrorString(const QString &err)
{
    _errors.append(err);
}

QString SyncResult::errorString() const
{
    if (_errors.isEmpty())
        return QString();
    return _errors.first();
}

void SyncResult::clearErrors()
{
    _errors.clear();
}

void SyncResult::processCompletedItem(const SyncFileItemPtr &item)
{
    if (Progress::isWarningKind(item->_status)) {
        // Count any error conditions, error strings will have priority anyway.
        _foundFilesNotSynced = true;
    }

    if (item->isDirectory()
        && (item->instruction() & (CSYNC_INSTRUCTION_NEW | CSYNC_INSTRUCTION_TYPE_CHANGE | CSYNC_INSTRUCTION_REMOVE | CSYNC_INSTRUCTION_RENAME))) {
        _folderStructureWasChanged = true;
    }

    // Process the item to the gui
    if (item->_status == SyncFileItem::FatalError || item->_status == SyncFileItem::NormalError) {
        //: this displays an error string (%2) for a file %1
        appendErrorString(QObject::tr("%1: %2").arg(item->_file, item->_errorString));
        _numErrorItems++;
        if (!_firstItemError) {
            _firstItemError = item;
        }
    } else if (item->_status == SyncFileItem::Conflict) {
        if (item->instruction() == CSYNC_INSTRUCTION_CONFLICT) {
            _numNewConflictItems++;
            if (!_firstNewConflictItem) {
                _firstNewConflictItem = item;
            }
        } else {
            _numOldConflictItems++;
        }
    } else {
        if (!item->hasErrorStatus() && item->_status != SyncFileItem::FileIgnored && item->_direction == SyncFileItem::Down) {
            switch (item->instruction()) {
            case CSYNC_INSTRUCTION_NEW:
            case CSYNC_INSTRUCTION_TYPE_CHANGE:
                _numNewItems++;
                if (!_firstItemNew)
                    _firstItemNew = item;
                break;
            case CSYNC_INSTRUCTION_REMOVE:
                _numRemovedItems++;
                if (!_firstItemDeleted)
                    _firstItemDeleted = item;
                break;
            case CSYNC_INSTRUCTION_SYNC:
                _numUpdatedItems++;
                if (!_firstItemUpdated)
                    _firstItemUpdated = item;
                break;
            case CSYNC_INSTRUCTION_RENAME:
                if (!_firstItemRenamed) {
                    _firstItemRenamed = item;
                }
                _numRenamedItems++;
                break;
            default:
                // nothing.
                break;
            }
        } else if (item->instruction() == CSYNC_INSTRUCTION_IGNORE && item->_hasBlacklistEntry) {
            if (item->_hasBlacklistEntry) {
                _numBlacklistErrors++;
            }
            _foundFilesNotSynced = true;
        }
    }
}

int SyncResult::numBlacklistErrors() const
{
    return _numBlacklistErrors;
}

} // ns mirall
