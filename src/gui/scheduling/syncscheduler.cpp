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
#include "libsync/configfile.h"
#include "libsync/syncengine.h"

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcSyncScheduler, "gui.scheduler.syncscheduler", QtInfoMsg)

class FolderPriorityQueue
{
public:
    FolderPriorityQueue() = default;

    void enqueueFolder(Folder *folder, SyncScheduler::Priority priority)
    {
        const auto [it, inserted] = _scheduledFolders.emplace(folder, priority);
        if (inserted) {
            // the folder is not yet scheduled
            _queue.emplace(folder, priority);
        } else {
            // if the new priority is higher we need to rebuild the queue
            if (priority > it->second) {
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
                // replace the the old priority
                _scheduledFolders[folder] = priority;
            }
        }
    }

    auto empty() { return _queue.empty(); }
    auto size() { return _queue.size(); }

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

    // Normal syncs are performed incremental but when fullLocalDiscoveryInterval times out
    // a complete local discovery is performed.
    // This timer here triggers a sync independent of etag changes on the server.
    auto *fullLocalDiscoveryTimer = new QTimer(this);
    fullLocalDiscoveryTimer->setInterval(ConfigFile().fullLocalDiscoveryInterval() + 2min);
    connect(fullLocalDiscoveryTimer, &QTimer::timeout, this, [parent, this] {
        for (auto *f : parent->folders()) {
            if (f->isReady() && f->accountState()->state() == AccountState::State::Connected) {
                enqueueFolder(f);
            }
        }
    });
    fullLocalDiscoveryTimer->start();
}


SyncScheduler::~SyncScheduler()
{
    delete _queue;
}

void SyncScheduler::enqueueFolder(Folder *folder, Priority priority)
{
    Q_ASSERT(folder->isReady());
    Q_ASSERT(folder->canSync());
    qCInfo(lcSyncScheduler) << "Enqueue" << folder->path() << priority << "QueueSize:" << _queue->size();
    _queue->enqueueFolder(folder, priority);
    if (!_currentSync) {
        startNext();
    }
}

void SyncScheduler::startNext()
{
    if (!_running) {
        qCInfo(lcSyncScheduler) << "Scheduler is paused, next sync is not started";
        return;
    }

    if (!_currentSync.isNull()) {
        qCInfo(lcSyncScheduler) << "Another sync is already running, waiting for that to finish before starting a new sync";
        return;
    }

    auto nextSync = _queue->pop();
    while (nextSync && !nextSync->canSync()) {
        nextSync = _queue->pop();
    }
    _currentSync = nextSync;
    if (!_currentSync.isNull()) {
        connect(
            _currentSync, &Folder::syncFinished, this,
            [this](const SyncResult &result) {
                qCInfo(lcSyncScheduler) << "Sync finished for" << _currentSync->path() << "with status" << result.status();
                if (result.status() != SyncResult::Success) {
                    auto reschedule = [this]() {
                        QTimer::singleShot(SyncEngine::minimumFileAgeForUpload, this, [folder = _currentSync, this] {
                            if (folder->canSync()) {
                                enqueueFolder(folder);
                            } else {
                                qCInfo(lcSyncScheduler) << "Cannot schedule sync for" << folder->path();
                            }
                        });
                    };

                    // Retry a couple of times after failure; or regularly if requested
                    if (_currentSync->consecutiveFailingSyncs() > 0) {
                        if (_currentSync->consecutiveFailingSyncs() < 3) {
                            qCInfo(lcSyncScheduler) << "Sync failed, rescheduling sync for" << _currentSync->path();
                            reschedule();
                        } else {
                            qCInfo(lcSyncScheduler) << "Sync for" << _currentSync->path() << "failed 3 times, not rescheduling";
                        }
                    } else if (_currentSync->syncEngine().isAnotherSyncNeeded() == AnotherSyncNeeded::DelayedFollowUp) {
                        qCInfo(lcSyncScheduler) << "Delayed follow-up needed, rescheduling sync for" << _currentSync->path();
                        reschedule();
                    }
                }
                _currentSync = nullptr;
                startNext();
            },
            Qt::SingleShotConnection);
        connect(_currentSync, &Folder::destroyed, this, &SyncScheduler::startNext, Qt::SingleShotConnection);
        qCInfo(lcSyncScheduler) << "Starting sync for" << _currentSync->path();
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
