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

#include "jobqueue.h"

#include "abstractnetworkjob.h"
#include "account.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcJobQUeue, "sync.networkjob.jobqueue", QtDebugMsg)

JobQueue::JobQueue(Account *account)
    : _account(account)
{
}

void JobQueue::block()
{
    _blocked++;
    qCDebug(lcJobQUeue) << "block:" << _blocked << _account->displayNameWithHost();
}

void JobQueue::unblock()
{
    if (!isBlocked()) {
        return;
    }
    _blocked--;
    qCDebug(lcJobQUeue) << "unblock:" << _blocked << _account->displayNameWithHost();
    if (_blocked == 0) {
        auto tmp = std::move(_jobs);
        for (auto job : tmp) {
            if (job) {
                qCDebug(lcJobQUeue) << "Retry" << job;
                job->retry();
            }
        }
    }
}

bool JobQueue::isBlocked() const
{
    return _blocked != 0;
}

bool JobQueue::retry(AbstractNetworkJob *job)
{
    if (!job->needsRetry()) {
        return false;
    }
    if (_blocked) {
        qCDebug(lcJobQUeue) << "Retry queued" << job;
        _jobs.push_back(job);
    } else {
        qCDebug(lcJobQUeue) << "Direct retry" << job;
        job->retry();
    }
    return true;
}

bool JobQueue::enqueue(AbstractNetworkJob *job)
{
    if (!_blocked) {
        return false;
    }
    qCDebug(lcJobQUeue) << "Queue" << job;
    _jobs.push_back(job);
    return true;
}

void JobQueue::clear()
{
    _blocked = 0;
    auto tmp = std::move(_jobs);
    for (auto job : tmp) {
        if (job) {
            qCDebug(lcJobQUeue) << "Abort" << job;
            job->abort();
        }
    }
}

size_t JobQueue::size() const
{
    return _jobs.size();
}

JobQueueGuard::JobQueueGuard(JobQueue *queue)
    : _queue(queue)
{
}

JobQueueGuard::~JobQueueGuard()
{
    unblock();
}

bool JobQueueGuard::block()
{
    if (!_blocked) {
        _blocked = true;
        _queue->block();
        return true;
    }
    return false;
}

bool JobQueueGuard::unblock()
{
    if (_blocked) {
        _blocked = false;
        _queue->unblock();
        return true;
    }
    return false;
}

bool JobQueueGuard::clear()
{
    if (_blocked) {
        _blocked = false;
        _queue->clear();
        return true;
    }
    return false;
}

JobQueue *JobQueueGuard::queue() const
{
    return _queue;
}
}
