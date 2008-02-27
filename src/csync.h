/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006-2008 by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
 */

/**
 * @file csync.h
 *
 * @brief Application developer interface for csync.
 *
 * @defgroup csyncPublicAPI csync public API
 *
 * @{
 */

#ifndef _CSYNC_H
#define _CSYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * csync version information
 */
#define CSYNC_VERSION_MAJOR 0
#define CSYNC_VERSION_MINOR 1
#define CSYNC_VERSION_PATCH 0
#define CSYNC_VERSION_STRING "csync version 0.1.0"

#undef __P
#define __P(protos)   protos

/**
 * How deep to scan directories.
 */
#define MAX_DEPTH 50

/**
 * Maximum time difference between two replicas in seconds
 */
#define MAX_TIME_DIFFERENCE 10

/*
 * csync file declarations
 */
#define CSYNC_CONF_DIR ".csync"
#define CSYNC_CONF_FILE "csync.conf"
#define CSYNC_LOG_FILE "csync_log.conf"
#define CSYNC_EXCLUDE_FILE "csync_exclude.conf"
#define CSYNC_JOURNAL_FILE "csync_journal.db"
#define CSYNC_LOCK_FILE "lock"

/*
 * Forward declarations
 */
struct csync_s; typedef struct csync_s CSYNC;
struct csync_config_s; typedef struct csync_config_s csync_config_t;
struct csync_internal_s; typedef struct csync_internal_s csync_internal_t;

/**
 * @brief csync public structure
 */
struct csync_s {
  int (*init) __P((CSYNC *));
  int (*update) __P((CSYNC *));
  int (*reconcile) __P((CSYNC *));
  int (*propagate) __P((CSYNC *));
  int (*destroy) __P((CSYNC *));
  const char *(*version) __P((void));

  struct {
    int max_depth;
    int max_time_difference;
    char *config_dir;
  } options;

  csync_internal_t *internal;
};


/**
 * @brief Allocate a csync context.
 *
 * @param ctx context variable to allocate
 *
 * @return 0 on success, less than 0 if an error occured with errno set.
 */
int csync_create __P((CSYNC **));

int csync_init __P((CSYNC *));

int csync_update __P((CSYNC *));

int csync_reconcile __P((CSYNC *));

int csync_propagate __P((CSYNC *));

int csync_destroy __P((CSYNC *));

const char *csync_version __P((void));

#ifdef __cplusplus
}
#endif

/**
 * }@
 */
#endif /* _CSYNC_H */

