/*
 * c_time - time functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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

#ifndef _C_TIME_H
#define _C_TIME_H

#include "ocsynclib.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

/**
 * @brief Calculate time difference
 *
 * The c_tspecdiff function returns the time elapsed between time time1 and time
 * time0 represented as timespec.
 *
 * @param time1 The time.
 * @param time0 The time.
 *
 * @return time elapsed between time1 and time0.
 */
struct timespec c_tspecdiff(struct timespec time1, struct timespec time0);

/**
 * @brief Calculate time difference.
 *
 * The function returns the time elapsed between time clock1 and time
 * clock0 represented as double (in seconds and milliseconds).
 *
 * @param clock1 The time.
 * @param clock0 The time.
 *
 * @return time elapsed between clock1 and clock0 in seconds and
 *         milliseconds.
 */
double c_secdiff(struct timespec clock1, struct timespec clock0);

OCSYNC_EXPORT int c_utimes(const char *uri, const struct timeval *times);

#ifdef __cplusplus
}
#endif

#endif /* _C_TIME_H */
