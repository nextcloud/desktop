/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006 by Andreas Schneider <mail@cynapses.org>
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

#include "config.h"

#ifdef CSYNC_TEST
#undef WITH_LOG4C
#endif

#ifdef WITH_LOG4C
#include "log4c.h"
#else
#include <stdarg.h>
#include <stdio.h>
#endif

#ifndef CSYNC_LOG_CATEGORY_NAME
#define CSYNC_LOG_CATEGORY_NAME "root"
#endif

/* GCC have printf type attribute check.  */
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif /* __GNUC__ */

#define CSYNC_LOG(priority, fmt, ...) \
  csync_log((char *) CSYNC_LOG_CATEGORY_NAME, priority, fmt, ## __VA_ARGS__)

#ifdef WITH_LOG4C
#define CSYNC_LOG_PRIORITY_FATAL   LOG4C_PRIORITY_FATAL
#define CSYNC_LOG_PRIORITY_ALERT   LOG4C_PRIORITY_ALERT
#define CSYNC_LOG_PRIORITY_CRIT    LOG4C_PRIORITY_CRIT
#define CSYNC_LOG_PRIORITY_ERROR   LOG4C_PRIORITY_ERROR
#define CSYNC_LOG_PRIORITY_WARN    LOG4C_PRIORITY_WARN
#define CSYNC_LOG_PRIORITY_NOTICE  LOG4C_PRIORITY_NOTICE
#define CSYNC_LOG_PRIORITY_INFO    LOG4C_PRIORITY_INFO
#define CSYNC_LOG_PRIORITY_DEBUG   LOG4C_PRIORITY_DEBUG
#define CSYNC_LOG_PRIORITY_TRACE   LOG4C_PRIORITY_TRACE
#define CSYNC_LOG_PRIORITY_NOTSET  LOG4C_PRIORITY_NOTSET
#define CSYNC_LOG_PRIORITY_UNKNOWN LOG4C_PRIORITY_UNKNOWN
#else
#define LOG4C_INLINE inline
#define CSYNC_LOG_PRIORITY_FATAL   000
#define CSYNC_LOG_PRIORITY_ALERT   100
#define CSYNC_LOG_PRIORITY_CRIT    200
#define CSYNC_LOG_PRIORITY_ERROR   300
#define CSYNC_LOG_PRIORITY_WARN    500
#define CSYNC_LOG_PRIORITY_NOTICE  500
#define CSYNC_LOG_PRIORITY_INFO    600
#define CSYNC_LOG_PRIORITY_DEBUG   700
#define CSYNC_LOG_PRIORITY_TRACE   800
#define CSYNC_LOG_PRIORITY_NOTSET  900
#define CSYNC_LOG_PRIORITY_UNKNOWN 1000
#endif

static LOG4C_INLINE void csync_log(char *catName, int a_priority,
    const char* a_format,...) PRINTF_ATTRIBUTE(3, 4);
/**
 * @brief The constructor of the logging mechanism
 *
 * @return  0 on success, less than 0 if an error occured.
 */
static LOG4C_INLINE int csync_log_init() {
#ifdef WITH_LOG4C
  return log4c_init();
#else
  return 0;
#endif
}

/**
 * @brief Load resource configuration file
 *
 * @param Path to the file to load
 *
 * @return  0 on success, less than 0 if an error occured.
 **/
static LOG4C_INLINE int csync_log_load(const char *path){
#ifdef WITH_LOG4C
  return log4c_load(path);
#else
  if (path == NULL) {
    return 0;
  }
  return 0;
#endif
}

/**
 * @brief The destructor of the logging mechanism
 *
 * @return  0 on success, less than 0 if an error occured.
 */
static LOG4C_INLINE int csync_log_fini(){
#ifdef WITH_LOG4C
  return log4c_fini();
#else
  return 0;
#endif
}

static LOG4C_INLINE int csync_log_setappender(char *catName, char *appName) {
#ifdef WITH_LOG4C
  log4c_category_set_appender(log4c_category_get(catName),
      log4c_appender_get(appName));
  return 0;
#else
  if (catName == NULL || appName == NULL) {
    return 0;
  }
  return 0;
#endif
}

static LOG4C_INLINE void csync_log(char *catName, int a_priority,
    const char* a_format,...) {
#ifdef WITH_LOG4C
  const log4c_category_t* a_category = log4c_category_get(catName);
  if (log4c_category_is_priority_enabled(a_category, a_priority)) {
    va_list va;
    va_start(va, a_format);
    log4c_category_vlog(a_category, a_priority, a_format, va);
    va_end(va);
  }
#else
  va_list va;
  va_start(va, a_format);
  if (a_priority > 0) {
    printf("%s - ", catName);
  }
  vprintf(a_format, va);
  va_end(va);
  printf("\n");
#endif
}

/**
 * }@
 */
#endif /* _CSYNC_LOG_H */

/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
