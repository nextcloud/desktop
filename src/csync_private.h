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
 *
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
 */

/**
 * @file csync_private.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncInternalAPI csync internal API
 *
 * @{
 */

#ifndef _CSYNC_PRIVATE_H
#define _CSYNC_PRIVATE_H

#include <stdint.h>
#include <sqlite3.h>

#include "config.h"
#include "c_lib.h"
#include "csync.h"

#include "vio/csync_vio_method.h"
#include "csync_macros.h"

/**
 * How deep to scan directories.
 */
#define MAX_DEPTH 50

/**
 * Maximum time difference between two replicas in seconds
 */
#define MAX_TIME_DIFFERENCE 10

/**
 * Maximum size of a buffer for transfer
 */
#ifndef MAX_XFER_BUF_SIZE
#define MAX_XFER_BUF_SIZE (16 * 1024)
#endif

#define CSYNC_STATUS_INIT 1 << 0
#define CSYNC_STATUS_UPDATE 1 << 1
#define CSYNC_STATUS_RECONCILE 1 << 2
#define CSYNC_STATUS_PROPAGATE 1 << 3
#define CSYNC_STATUS_DONE (CSYNC_STATUS_INIT|CSYNC_STATUS_UPDATE|CSYNC_STATUS_RECONCILE|CSYNC_STATUS_PROPAGATE)

enum csync_replica_e {
  LOCAL_REPLICA,
  REMOTE_REPLCIA
};

/**
 * @brief csync public structure
 */
struct csync_s {
  csync_auth_callback auth_callback;
  c_strlist_t *excludes;

  struct {
    char *file;
    sqlite3 *db;
    int exists;
    int disabled;
  } statedb;

  struct {
    char *uri;
    c_rbtree_t *tree;
    c_list_t *list;
    enum csync_replica_e type;
  } local;

  struct {
    char *uri;
    c_rbtree_t *tree;
    c_list_t *list;
    enum csync_replica_e type;
  } remote;

  struct {
    void *handle;
    csync_vio_method_t *method;
    csync_vio_method_finish_fn finish_fn;
  } module;

  struct {
    int max_depth;
    int max_time_difference;
    int sync_symbolic_links;
    char *config_dir;
  } options;

  enum csync_replica_e current;
  enum csync_replica_e replica;

  int status;
};

enum csync_ftw_type_e {
  CSYNC_FTW_TYPE_FILE,
  CSYNC_FTW_TYPE_SLINK,
  CSYNC_FTW_TYPE_DIR
};

enum csync_instructions_e {
  CSYNC_INSTRUCTION_NONE,
  CSYNC_INSTRUCTION_EVAL,
  CSYNC_INSTRUCTION_REMOVE,
  CSYNC_INSTRUCTION_RENAME,
  CSYNC_INSTRUCTION_NEW,
  CSYNC_INSTRUCTION_CONFLICT,
  CSYNC_INSTRUCTION_IGNORE,
  CSYNC_INSTRUCTION_SYNC,
  CSYNC_INSTRUCTION_STAT_ERROR,
  CSYNC_INSTRUCTION_ERROR,
  /* instructions for the propagator */
  CSYNC_INSTRUCTION_DELETED,
  CSYNC_INSTRUCTION_UPDATED
};

typedef struct csync_file_stat_s {
  ino_t inode;
  uid_t uid;
  gid_t gid;
  mode_t mode;
  off_t size;
  int nlink;
  time_t modtime;
  int type;
  enum csync_instructions_e instruction;
  uint64_t phash;
  size_t pathlen;
  char path[1];
} csync_file_stat_t;

/**
 * }@
 */
#endif /* _CSYNC_PRIVATE_H */

