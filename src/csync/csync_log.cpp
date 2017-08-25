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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "csync_private.h"
#include "csync_log.h"

CSYNC_THREAD int csync_log_level;
CSYNC_THREAD csync_log_callback csync_log_cb;

void csync_log(int verbosity,
               const char *function,
               const char *format, ...)
{
    csync_log_callback log_fn = csync_get_log_callback();
    if (log_fn && verbosity <= csync_get_log_level()) {
        char buffer[1024];
        va_list va;
 
        va_start(va, format);
        vsnprintf(buffer, sizeof(buffer), format, va);
        va_end(va);

        log_fn(verbosity, function, buffer);
        return;
    }
}

int csync_set_log_level(int level) {
  if (level < 0) {
    return -1;
  }

  csync_log_level = level;

  return 0;
}

int csync_get_log_level(void) {
  return csync_log_level;
}

int csync_set_log_callback(csync_log_callback cb) {
  if (cb == NULL) {
    return -1;
  }

  csync_log_cb = cb;

  return 0;
}

csync_log_callback csync_get_log_callback(void) {
  return csync_log_cb;
}
