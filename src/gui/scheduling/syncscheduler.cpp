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

#include "gui/scheduling/syncscheduler.h"

#include "gui/folderman.h"
#include "gui/scheduling/etagwatcher.h"

using namespace OCC;


class FolderPriorityQueue
{
public:
    FolderPriorityQueue() = default;

    void enqueueFolder(Folder *folder, SyncScheduler::Priority priority)
    {
        auto old = _scheduledFolders.find(folder);
        if (old == _scheduledFolders.cend()) {
            // the folder is not yet scheduled
            _queue.emplace(folder, priority);
        } else {
            _scheduledFolders.insert({folder, priority});
            // if the new priority is higher we need to rebuild the queue
            if (priority > old->second) {
                // we need to reorder the queue
                // this is expensive
                decltype(_queue) out;
                for (; !_queue.empty(); _queue.pop()) {
                    const auto &tmp = _queue.top();
                    if (tmp.folder != folder) {
                        out.push(std::move(tmp));
                    } else {
                        out.emplace(folder, priority);
                    }
                }
                _queue = std::move(out);
            }
        }
    }

    bool empty() { return _queue.empty(); }

    QPointer<Folder> pop()
    {
        QPointer<Folder> out;
        while (!_queue.empty() && !out) {
            // could be a nullptr by now
            out = _queue.top().folder;
            _scheduledFolders.erase(_queue.top().rawFolder);
            _queue.pop();
        }
        return out;
    }

private:
    struct Element
    {
        Element(Folder *f, SyncScheduler::Priority p)
            : folder(f)
            , rawFolder(f)
            , priority(p)
        {
        }

        // We don't own the folder, so it might get deleted
        QPointer<Folder> folder;
        // raw pointer for lookup in _scheduledFolders
        Folder *rawFolder;
        SyncScheduler::Priority priority;

        friend bool operator<(const Element &lhs, const Element &rhs) { return lhs.priority < rhs.priority; }
    };

    // the actual queue
    std::priority_queue<Element> _queue;
    // helper container to ensure we don't enqueue a Folder multiple times
    std::unordered_map<Folder *, SyncScheduler::Priority> _scheduledFolders;
};

SyncScheduler::SyncScheduler(FolderMan *parent)
    : QObject(parent)
    , _queue(new FolderPriorityQueue)
{
    new ETagWatcher(parent, this);
}


SyncScheduler::~SyncScheduler()
{
    delete _queue;
}

void SyncScheduler::enqueueFolder(Folder *folder, Priority priority)
{
    Q_ASSERT(folder->isReady());
    Q_ASSERT(folder->canSync());
    _queue->enqueueFolder(folder, priority);
    if (_running) {
        startNext();
    }
}

void SyncScheduler::startNext()
{
    if (OC_ENSURE_NOT(!_running || _currentSync)) {
        return;
    }
    _currentSync = _queue->pop();
    if (_currentSync) {
        connect(
            _currentSync, &Folder::syncFinished, this,
            [this] {
                _currentSync = nullptr;
                if (_running) {
                    startNext();
                }
            },
            Qt::SingleShotConnection);
        connect(_currentSync, &Folder::destroyed, this, &SyncScheduler::startNext, Qt::SingleShotConnection);
        _currentSync->startSync();
    }
}

void SyncScheduler::start()
{
    _running = true;
    startNext();
}

void SyncScheduler::stop()
{
    _running = false;
}

bool SyncScheduler::hasCurrentRunningSyncRunning() const
{
    return _currentSync;
}
