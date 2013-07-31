/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Andreas Schneider <asn@cryptomilk.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#else
#include <sys/utime.h>
#endif
#include <time.h>

#include "csync_private.h"
#include "csync_log.h"

static int current_timestring(int hires, char *buf, size_t len)
{
    char tbuf[64];
    struct timeval tv;
    struct tm *tm;
    time_t t;

    gettimeofday(&tv, NULL);
    t = (time_t) tv.tv_sec;

    tm = localtime(&t);
    if (tm == NULL) {
        return -1;
    }

    if (hires) {
        strftime(tbuf, sizeof(tbuf) - 1, "%Y/%m/%d %H:%M:%S", tm);
        snprintf(buf, len, "%s.%06ld", tbuf, (long) tv.tv_usec);
    } else {
        strftime(tbuf, sizeof(tbuf) - 1, "%Y/%m/%d %H:%M:%S", tm);
        snprintf(buf, len, "%s", tbuf);
    }

    return 0;
}

static void csync_log_stderr(int verbosity,
                             const char *function,
                             const char *buffer)
{
    char date[64] = {0};
    int rc;

    rc = current_timestring(1, date, sizeof(date));
    if (rc == 0) {
        fprintf(stderr, "[%s, %d] %s:", date+5, verbosity, function);
    } else {
        fprintf(stderr, "[%d] %s", verbosity, function);
    }

    fprintf(stderr, "  %s\n", buffer);
}
static void csync_log_function(CSYNC *ctx,
                               int verbosity,
                               const char *function,
                               const char *buffer)
{
    csync_log_callback log_fn = csync_get_log_callback(ctx);
    if (log_fn) {
        char buf[1024];

        snprintf(buf, sizeof(buf), "%s: %s", function, buffer);

        log_fn(ctx,
               verbosity,
               function,
               buf,
               csync_get_userdata(ctx));
        return;
    }

    csync_log_stderr(verbosity, function, buffer);
}


void csync_log(CSYNC *ctx,
               int verbosity,
               const char *function,
               const char *format, ...)
{
    char buffer[1024];
    va_list va;

    if (ctx == NULL) {
        return;
    }

    if (verbosity <= csync_get_log_verbosity(ctx)) {
        va_start(va, format);
        vsnprintf(buffer, sizeof(buffer), format, va);
        va_end(va);
        csync_log_function(ctx, verbosity, function, buffer);
    }
}

