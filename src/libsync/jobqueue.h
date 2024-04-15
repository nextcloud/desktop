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

#include "owncloudlib.h"

#include <QPointer>
#include <vector>

namespace OCC {

class AbstractNetworkJob;
class Account;

class OWNCLOUDSYNC_EXPORT JobQueue
{
public:
    JobQueue(Account *account);

    /**
     * whether jobs need to be enqued
     */
    bool isBlocked() const;

    /**
     * Retry a job if the job allows it,
     * if blocked the job will be queued untill we are unblocked
     * Returns whether the job will be retired
     */
    bool retry(AbstractNetworkJob *job);
    /**
     * Enque if blocked
     * Returns whether the job was enqueued
     */
    bool enqueue(AbstractNetworkJob *job);

    size_t size() const;

    /**
     * Clear the queue and abort all jobs
     */
    void clear();

private:
    void block();
    void unblock();

    Account *_account;
    uint _blocked = 0;
    std::vector<QPointer<AbstractNetworkJob>> _jobs;

    friend class JobQueueGuard;
};

class OWNCLOUDSYNC_EXPORT JobQueueGuard
{
public:
    JobQueueGuard(JobQueue *queue);
    ~JobQueueGuard();

    bool block();
    bool unblock();
    bool clear();

    JobQueue *queue() const;

private:
    JobQueue *_queue;
    bool _blocked = false;
};
}
