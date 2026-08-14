/*
 * SPDX-FileCopyrightText: 2025 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudlib.h"

#include <QDebug>

#include <chrono>

namespace OCC::Utility {

/**
 * Meassure time using std::chrono::steady_clock
 */
class OWNCLOUDSYNC_EXPORT ChronoElapsedTimer
{
public:
    ChronoElapsedTimer(bool start = true);

    bool isStarted() const;
    /**
     * Resets and start the timer
     */
    void reset();
    /**
     * Stops the timer
     */
    void stop();
    /**
     * Returns the elapsed time.
     * If the timer is stopped it is the time between start and stop of the timer.
     */
    std::chrono::nanoseconds duration() const;

private:
    std::chrono::steady_clock::time_point _start = {};
    std::chrono::steady_clock::time_point _end = {};
    bool _started = false;
};

}

OWNCLOUDSYNC_EXPORT QDebug operator<<(QDebug debug, const OCC::Utility::ChronoElapsedTimer &in);
