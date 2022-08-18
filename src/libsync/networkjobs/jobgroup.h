/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#pragma once

#include "abstractnetworkjob.h"

#include <unordered_set>

namespace OCC {
/***
 * Utility to wait for multiple jobs
 */
class OWNCLOUDSYNC_EXPORT JobGroup : public QObject
{
    Q_OBJECT
public:
    JobGroup(QObject *parent);

    template <typename T, typename... Args>
    T *createJob(Args &&...args)
    {
        Q_ASSERT(!_finished);
        auto [it, created] = _jobs.emplace(new T(args...));
        Q_UNUSED(created);
        auto job = static_cast<T *>(*it);
        connect(job, &T::finishedSignal, this, [job, this] {
            _jobs.erase(job);
            if (_jobs.empty()) {
                // ensure we emit only once
                Q_ASSERT(!_finished);
                _finished = true;
                Q_EMIT finishedSignal();
            }
        });
        return job;
    }

    bool isEmpty() const;

Q_SIGNALS:
    void finishedSignal();

private:
    bool _finished = false;
    std::unordered_set<AbstractNetworkJob *> _jobs;
};

}