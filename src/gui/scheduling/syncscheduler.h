/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#pragma once

#include "etagwatcher.h"
#include "gui/folder.h"

#include <QObject>

#include <queue>
#include <unordered_map>


class FolderPriorityQueue;

namespace OCC {

class FolderMan;

class SyncScheduler : public QObject
{
    Q_OBJECT
public:
    enum class Priority : uint8_t {
        // Normal sync triggered by etag change or something similar
        Low,

        // Related to a user action
        Medium,

        // Usually triggered by a user (ForceSync)
        High
    };

    explicit SyncScheduler(FolderMan *parent);
    ~SyncScheduler() override;

    void enqueueFolder(Folder *folder, Priority priority = Priority::Low);

    void start();

    void stop();

    bool hasCurrentRunningSyncRunning() const;


private:
    void startNext();

    bool _running = false;
    QPointer<Folder> _currentSync;
    FolderPriorityQueue *_queue;
};
}
