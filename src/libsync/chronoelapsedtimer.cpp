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
#include "chronoelapsedtimer.h"

#include <QtGlobal>

using namespace OCC::Utility;
using namespace std::chrono;

ChronoElapsedTimer::ChronoElapsedTimer(bool start)
    : _start(steady_clock::now())
    , _started(start)
{
}

bool ChronoElapsedTimer::isStarted() const
{
    return _started;
}

void ChronoElapsedTimer::reset()
{
    _start = steady_clock::now();
    _end = {};
    _started = true;
}

void ChronoElapsedTimer::stop()
{
    Q_ASSERT(_end == steady_clock::time_point{});
    Q_ASSERT(_started);
    _end = steady_clock::now();
    // don't set _started to false, so that we can still call duration() later
}

nanoseconds ChronoElapsedTimer::duration() const
{
    if (!_started) {
        return nanoseconds::max();
    }
    if (_end != steady_clock::time_point{}) {
        return _end - _start;
    }
    return steady_clock::now() - _start;
}

QDebug operator<<(QDebug debug, const ChronoElapsedTimer &timer)
{
    QDebugStateSaver save(debug);
    debug.nospace();
    auto duration = timer.duration();
    const auto h = duration_cast<hours>(duration);
    const auto min = duration_cast<minutes>(duration -= h);
    const auto s = duration_cast<seconds>(duration -= min);
    const auto ms = duration_cast<milliseconds>(duration -= s);
    return debug << u"duration(" << h.count() << u"h, " << min.count() << u"min, " << s.count() << u"s, " << ms.count() << u"ms)";
}
