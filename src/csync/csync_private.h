/*
 * cynapses libc functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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
#include <stdbool.h>
#include <sqlite3.h>

#include "config_csync.h"
#include "std/c_lib.h"
#include "std/c_private.h"
#include "csync.h"
#include "csync_misc.h"

#include "csync_macros.h"

/**
 * How deep to scan directories.
 */
#define MAX_DEPTH 100

#define CSYNC_STATUS_INIT 1 << 0
#define CSYNC_STATUS_UPDATE 1 << 1
#define CSYNC_STATUS_RECONCILE 1 << 2
#define CSYNC_STATUS_PROPAGATE 1 << 3

#define CSYNC_STATUS_DONE (CSYNC_STATUS_INIT | \
                           CSYNC_STATUS_UPDATE | \
                           CSYNC_STATUS_RECONCILE | \
                           CSYNC_STATUS_PROPAGATE)

enum csync_replica_e {
  LOCAL_REPLICA,
  REMOTE_REPLICA
};

/**
 * @brief csync public structure
 */
struct csync_s {
  struct {
      csync_auth_callback auth_function;
      void *userdata;
      csync_update_callback update_callback;
      void *update_callback_userdata;

      /* hooks for checking the white list (uses the update_callback_userdata) */
      int (*checkSelectiveSyncBlackListHook)(void*, const char*);
      int (*checkSelectiveSyncNewFolderHook)(void*, const char* /* path */, const char* /* remotePerm */);


      csync_vio_opendir_hook remote_opendir_hook;
      csync_vio_readdir_hook remote_readdir_hook;
      csync_vio_closedir_hook remote_closedir_hook;
      void *vio_userdata;

      /* hook for comparing checksums of files during discovery */
      csync_checksum_hook checksum_hook;
      void *checksum_userdata;

  } callbacks;
  c_strlist_t *excludes;
  
  struct {
    char *file;
    sqlite3 *db;
    int exists;

    sqlite3_stmt* by_hash_stmt;
    sqlite3_stmt* by_fileid_stmt;
    sqlite3_stmt* by_inode_stmt;

    int lastReturnValue;
  } statedb;

  struct {
    char *uri;
    c_rbtree_t *tree;
  } local;

  struct {
    c_rbtree_t *tree;
    int  read_from_db;
    const char *root_perms; /* Permission of the root folder. (Since the root folder is not in the db tree, we need to keep a separate entry.) */
  } remote;


  /* replica we are currently walking */
  enum csync_replica_e current;

  /* Used in the update phase so changes in the sub directories can be notified to
     parent directories */
  csync_file_stat_t *current_fs;

  /* csync error code */
  enum csync_status_codes_e status_code;

  char *error_string;

  int status;
  volatile int abort;
  void *rename_info;

  /**
   * Specify if it is allowed to read the remote tree from the DB (default to enabled)
   */
  bool read_remote_from_db;

  /**
   * If true, the DB is considered empty and all reads are skipped. (default is false)
   * This is useful during the initial local discovery as it speeds it up significantly.
   */
  bool db_is_empty;

  bool ignore_hidden_files;
};

/*
 * context for the treewalk function
 */
struct _csync_treewalk_context_s
{
    csync_treewalk_visit_func *user_visitor;
    int instruction_filter;
    void *userdata;
};
typedef struct _csync_treewalk_context_s _csync_treewalk_context;

void set_errno_from_http_errcode( int err );

/**
 * }@
 */
#endif /* _CSYNC_PRIVATE_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
