/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006-2012 by Andreas Schneider <asn@cryptomilk.org>
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

#include "config.h"
#include "c_lib.h"
#include "c_private.h"
#include "csync.h"

#ifdef WITH_ICONV
#include <iconv.h>
#endif

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

#define CSYNC_STATUS_DONE (CSYNC_STATUS_INIT | \
                           CSYNC_STATUS_UPDATE | \
                           CSYNC_STATUS_RECONCILE | \
                           CSYNC_STATUS_PROPAGATE)

enum csync_replica_e {
  LOCAL_REPLICA,
  REMOTE_REPLICA
};

typedef struct csync_file_stat_s csync_file_stat_t;


/**
 * @brief csync public structure
 */
struct csync_s {
  struct {
      csync_auth_callback auth_function;
      csync_log_callback log_function;
      csync_progress_callback progresscb;
      void *userdata;
  } callbacks;
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
    int  read_from_db;
  } remote;

  struct {
    void *handle;
    csync_vio_method_t *method;
    csync_vio_method_finish_fn finish_fn;
    csync_vio_capabilities_t capabilities;
  } module;

  struct {
    int max_depth;
    int max_time_difference;
    int sync_symbolic_links;
    int unix_extensions;
    char *config_dir;
    bool with_conflict_copys;
    bool local_only_mode;
    bool remote_push_atomar;
    int log_verbosity;
    int timeout;
#ifdef WITH_ICONV
    iconv_t iconv_cd;
#endif
  } options;

  struct {
    uid_t uid;
    uid_t euid;
  } pwd;

  struct csync_progressinfo_s *progress;

  /* replica we are currently walking */
  enum csync_replica_e current;

  /* replica we want to work on */
  enum csync_replica_e replica;

  /* Used in the update phase so changes in the sub directories can be notified to
     parent directories */
  csync_file_stat_t *current_fs;

  /* error code of the last operation */
  enum csync_error_codes_e error_code;
  char *error_string;
  
  int status;
  volatile int abort;
  void *rename_info;
};


#ifdef _MSC_VER
#pragma pack(1)
#endif
struct csync_file_stat_s {
  uint64_t phash;   /* u64 */
  time_t modtime;   /* u64 */
  off_t size;       /* u64 */
  size_t pathlen;   /* u64 */
  uint64_t inode;   /* u64 */
  uid_t uid;        /* u32 */
  gid_t gid;        /* u32 */
  mode_t mode;      /* u32 */
  int nlink;        /* u32 */
  int type;         /* u32 */
  int child_modified;/*bool*/
  int should_update_md5; /*bool */

  char *destpath;   /* for renames */
  const char *md5;
  const char *error_string;

  enum csync_instructions_e instruction; /* u32 */
  char path[1]; /* u8 */
}
#if !defined(__SUNPRO_C) && !defined(_MSC_VER)
__attribute__ ((packed))
#endif
#ifdef _MSC_VER
#pragma pack()
#endif
;

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

/**
 * }@
 */
#endif /* _CSYNC_PRIVATE_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
