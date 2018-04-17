/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "localdiscoverytracker.h"

#include "syncfileitem.h"

#include <QLoggingCategory>

using namespace OCC;

Q_LOGGING_CATEGORY(lcLocalDiscoveryTracker, "sync.localdiscoverytracker", QtInfoMsg)

LocalDiscoveryTracker::LocalDiscoveryTracker()
{
}

void LocalDiscoveryTracker::addTouchedPath(const QByteArray &relativePath)
{
    qCDebug(lcLocalDiscoveryTracker) << "inserted touched" << relativePath;
    _localDiscoveryPaths.insert(relativePath);
}

void LocalDiscoveryTracker::startSyncFullDiscovery()
{
    _localDiscoveryPaths.clear();
    _previousLocalDiscoveryPaths.clear();
    qCDebug(lcLocalDiscoveryTracker) << "full discovery";
}

void LocalDiscoveryTracker::startSyncPartialDiscovery()
{
    if (lcLocalDiscoveryTracker().isDebugEnabled()) {
        QByteArrayList paths;
        for (auto &path : _localDiscoveryPaths)
            paths.append(path);
        qCDebug(lcLocalDiscoveryTracker) << "partial discovery with paths: " << paths;
    }

    _previousLocalDiscoveryPaths = std::move(_localDiscoveryPaths);
    _localDiscoveryPaths.clear();
}

const std::set<QByteArray> &LocalDiscoveryTracker::localDiscoveryPaths() const
{
    return _localDiscoveryPaths;
}

void LocalDiscoveryTracker::slotItemCompleted(const SyncFileItemPtr &item)
{
    // For successes, we want to wipe the file from the list to ensure we don't
    // rediscover it even if this overall sync fails.
    //
    // For failures, we want to add the file to the list so the next sync
    // will be able to retry it.
    if (item->_status == SyncFileItem::Success
        || item->_status == SyncFileItem::FileIgnored
        || item->_status == SyncFileItem::Restoration
        || item->_status == SyncFileItem::Conflict
        || (item->_status == SyncFileItem::NoStatus
               && (item->_instruction == CSYNC_INSTRUCTION_NONE
                      || item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA))) {
        if (_previousLocalDiscoveryPaths.erase(item->_file.toUtf8()))
            qCDebug(lcLocalDiscoveryTracker) << "wiped successful item" << item->_file;
    } else {
        _localDiscoveryPaths.insert(item->_file.toUtf8());
        qCDebug(lcLocalDiscoveryTracker) << "inserted error item" << item->_file;
    }
}

void LocalDiscoveryTracker::slotSyncFinished(bool success)
{
    if (success) {
        qCDebug(lcLocalDiscoveryTracker) << "sync success, forgetting last sync's local discovery path list";
    } else {
        // On overall-failure we can't forget about last sync's local discovery
        // paths yet, reuse them for the next sync again.
        // C++17: Could use std::set::merge().
        _localDiscoveryPaths.insert(
            _previousLocalDiscoveryPaths.begin(), _previousLocalDiscoveryPaths.end());
        qCDebug(lcLocalDiscoveryTracker) << "sync failed, keeping last sync's local discovery path list";
    }
    _previousLocalDiscoveryPaths.clear();
}
