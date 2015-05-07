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

/**
 * @file csync_log.h
 *
 * @brief Logging interface of csync
 *
 * @defgroup csyncLogInternals csync logging internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

#ifndef _CSYNC_LOG_H
#define _CSYNC_LOG_H

/* GCC have printf type attribute check.  */
#ifndef PRINTF_ATTRIBUTE
#ifdef __GNUC__
#ifdef _WIN32
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__gnu_printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#endif
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif /* __GNUC__ */
#endif /* ndef PRINTF_ATTRIBUTE */

enum csync_log_priority_e {
    CSYNC_LOG_PRIORITY_NOLOG = 0,
    CSYNC_LOG_PRIORITY_FATAL,
    CSYNC_LOG_PRIORITY_ALERT,
    CSYNC_LOG_PRIORITY_CRIT,
    CSYNC_LOG_PRIORITY_ERROR,
    CSYNC_LOG_PRIORITY_WARN,
    CSYNC_LOG_PRIORITY_NOTICE,
    CSYNC_LOG_PRIORITY_INFO,
    CSYNC_LOG_PRIORITY_DEBUG,
    CSYNC_LOG_PRIORITY_TRACE,
    CSYNC_LOG_PRIORITY_NOTSET,
    CSYNC_LOG_PRIORITY_UNKNOWN,
};

#define CSYNC_LOG(priority, ...) \
  csync_log(priority, __func__, __VA_ARGS__)

void csync_log(int verbosity,
               const char *function,
               const char *format, ...) PRINTF_ATTRIBUTE(3, 4);

/**
 * }@
 */
#endif /* _CSYNC_LOG_H */

/* vim: set ft=c.doxygen ts=4 sw=4 et cindent: */
