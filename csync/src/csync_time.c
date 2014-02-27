/*
 * libcsync -- a library to sync a directory with another
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "csync_time.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.time"
#include "csync_log.h"

#ifdef HAVE_CLOCK_GETTIME
# ifdef _POSIX_MONOTONIC_CLOCK
#  define CSYNC_CLOCK CLOCK_MONOTONIC
# else
#  define CSYNC_CLOCK CLOCK_REALTIME
# endif
#endif


int csync_gettime(struct timespec *tp)
{
#ifdef HAVE_CLOCK_GETTIME
	return clock_gettime(CSYNC_CLOCK, tp);
#else
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		return -1;
	}

	tp->tv_sec = tv.tv_sec;
	tp->tv_nsec = tv.tv_usec * 1000;
#endif
	return 0;
}

#undef CSYNC_CLOCK

/* vim: set ts=8 sw=2 et cindent: */
