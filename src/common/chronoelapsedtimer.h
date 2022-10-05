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
#include "ocsynclib.h"

#include <chrono>

namespace OCC::Utility {

/**
 * Meassure time using std::chrono::steady_clock
 */
class OCSYNC_EXPORT ChronoElapsedTimer
{
public:
    ChronoElapsedTimer();

    /**
     * Resets the timer
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
};

}
