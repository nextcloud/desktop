/*
 * libcsync -- a library to sync a directory with another
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_statedb.h"
#include "csync_time.h"
#include "csync_util.h"
#include "csync_misc.h"
#include "std/c_private.h"

#include "csync_update.h"
#include "csync_reconcile.h"

#include "vio/csync_vio.h"

#include "csync_log.h"
#include "csync_rename.h"
#include "c_jhash.h"

static int _key_cmp(const void *key, const void *data) {
  uint64_t a;
  csync_file_stat_t *b;

  a = *(uint64_t *) (key);
  b = (csync_file_stat_t *) data;

  if (a < b->phash) {
    return -1;
  } else if (a > b->phash) {
    return 1;
  }

  return 0;
}

static int _data_cmp(const void *key, const void *data) {
  csync_file_stat_t *a, *b;

  a = (csync_file_stat_t *) key;
  b = (csync_file_stat_t *) data;

  if (a->phash < b->phash) {
    return -1;
  } else if (a->phash > b->phash) {
    return 1;
  }

  return 0;
}

void csync_create(CSYNC **csync, const char *local) {
  CSYNC *ctx;
  size_t len = 0;

  ctx = (CSYNC*)c_malloc(sizeof(CSYNC));

  ctx->status_code = CSYNC_STATUS_OK;

  /* remove trailing slashes */
  len = strlen(local);
  while(len > 0 && local[len - 1] == '/') --len;

  ctx->local.uri = c_strndup(local, len);

  ctx->status_code = CSYNC_STATUS_OK;

  ctx->current_fs = NULL;

  ctx->abort = false;

  ctx->ignore_hidden_files = true;

  *csync = ctx;
}

void csync_init(CSYNC *ctx, const char *db_file) {
  assert(ctx);
  /* Do not initialize twice */

  assert(!(ctx->status & CSYNC_STATUS_INIT));
  ctx->status_code = CSYNC_STATUS_OK;

  ctx->local.type = LOCAL_REPLICA;

  ctx->remote.type = REMOTE_REPLICA;

  SAFE_FREE(ctx->statedb.file);
  ctx->statedb.file = c_strdup(db_file);

  c_rbtree_create(&ctx->local.tree, _key_cmp, _data_cmp);
  c_rbtree_create(&ctx->remote.tree, _key_cmp, _data_cmp);

  ctx->remote.root_perms = 0;

  ctx->status = CSYNC_STATUS_INIT;

  /* initialize random generator */
  srand(time(NULL));
}

int csync_update(CSYNC *ctx) {
  int rc = -1;
  struct timespec start, finish;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }
  ctx->status_code = CSYNC_STATUS_OK;

  /* Path of database file is set in csync_init */
  if (csync_statedb_load(ctx, ctx->statedb.file, &ctx->statedb.db) < 0) {
      rc = -1;
      return rc;
  }

  ctx->status_code = CSYNC_STATUS_OK;

  csync_memstat_check();

  if (!ctx->excludes) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "No exclude file loaded or defined!");
  }

  /* update detection for local replica */
  csync_gettime(&start);
  ctx->current = LOCAL_REPLICA;
  ctx->replica = ctx->local.type;

  rc = csync_ftw(ctx, ctx->local.uri, csync_walker, MAX_DEPTH);
  if (rc < 0) {
    if(ctx->status_code == CSYNC_STATUS_OK) {
        ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
    }
    goto out;
  }

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for local replica took %.2f seconds walking %zu files.",
            c_secdiff(finish, start), c_rbtree_size(ctx->local.tree));
  csync_memstat_check();

  /* update detection for remote replica */
  csync_gettime(&start);
  ctx->current = REMOTE_REPLICA;
  ctx->replica = ctx->remote.type;

  rc = csync_ftw(ctx, "", csync_walker, MAX_DEPTH);
  if (rc < 0) {
      if(ctx->status_code == CSYNC_STATUS_OK) {
          ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
      }
      goto out;
  }

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for remote replica took %.2f seconds "
            "walking %zu files.",
            c_secdiff(finish, start), c_rbtree_size(ctx->remote.tree));
  csync_memstat_check();

  ctx->status |= CSYNC_STATUS_UPDATE;

  rc = 0;

out:
  csync_statedb_close(ctx);
  return rc;
}

int csync_reconcile(CSYNC *ctx) {
  int rc = -1;
  struct timespec start, finish;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }
  ctx->status_code = CSYNC_STATUS_OK;

  /* Reconciliation for local replica */
  csync_gettime(&start);

  if (csync_statedb_load(ctx, ctx->statedb.file, &ctx->statedb.db) < 0) {
    rc = -1;
    return rc;
  }

  ctx->current = LOCAL_REPLICA;
  ctx->replica = ctx->local.type;

  rc = csync_reconcile_updates(ctx);

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
      "Reconciliation for local replica took %.2f seconds visiting %zu files.",
      c_secdiff(finish, start), c_rbtree_size(ctx->local.tree));

  if (rc < 0) {
      if (!CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = csync_errno_to_status( errno, CSYNC_STATUS_RECONCILE_ERROR );
      }
      goto out;
  }

  /* Reconciliation for remote replica */
  csync_gettime(&start);

  ctx->current = REMOTE_REPLICA;
  ctx->replica = ctx->remote.type;

  rc = csync_reconcile_updates(ctx);

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
      "Reconciliation for remote replica took %.2f seconds visiting %zu files.",
      c_secdiff(finish, start), c_rbtree_size(ctx->remote.tree));

  if (rc < 0) {
      if (!CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = csync_errno_to_status(errno,  CSYNC_STATUS_RECONCILE_ERROR );
      }
      goto out;
  }

  ctx->status |= CSYNC_STATUS_RECONCILE;

  rc = 0;

out:
  csync_statedb_close(ctx);
  return 0;
}

/*
 * local visitor which calls the user visitor with repacked stat info.
 */
static int _csync_treewalk_visitor(void *obj, void *data) {
    int rc = 0;
    csync_file_stat_t *cur         = NULL;
    CSYNC *ctx                     = NULL;
    csync_treewalk_visit_func *visitor   = NULL;
    _csync_treewalk_context *twctx = NULL;
    c_rbtree_t *other_tree = NULL;
    c_rbnode_t *other_node = NULL;

    cur = (csync_file_stat_t *) obj;
    ctx = (CSYNC *) data;

    if (ctx == NULL) {
      return -1;
    }

    /* we need the opposite tree! */
    switch (ctx->current) {
    case LOCAL_REPLICA:
        other_tree = ctx->remote.tree;
        break;
    case REMOTE_REPLICA:
        other_tree = ctx->local.tree;
        break;
    default:
        break;
    }

    other_node = c_rbtree_find(other_tree, &cur->phash);

    if (!other_node) {
        /* Check the renamed path as well. */
        int len;
        uint64_t h = 0;
        char *renamed_path = csync_rename_adjust_path(ctx, cur->path);

        if (!c_streq(renamed_path, cur->path)) {
            len = strlen( renamed_path );
            h = c_jhash64((uint8_t *) renamed_path, len, 0);
            other_node = c_rbtree_find(other_tree, &h);
        }
        SAFE_FREE(renamed_path);
    }

    if (!other_node) {
        /* Check the source path as well. */
        int len;
        uint64_t h = 0;
        char *renamed_path = csync_rename_adjust_path_source(ctx, cur->path);

        if (!c_streq(renamed_path, cur->path)) {
            len = strlen( renamed_path );
            h = c_jhash64((uint8_t *) renamed_path, len, 0);
            other_node = c_rbtree_find(other_tree, &h);
        }
        SAFE_FREE(renamed_path);
    }

    csync_file_stat_t *other = other_node ? (csync_file_stat_t*)other_node->data : NULL;

    if (obj == NULL || data == NULL) {
      ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
      return -1;
    }
    ctx->status_code = CSYNC_STATUS_OK;

    twctx = (_csync_treewalk_context*) ctx->callbacks.userdata;
    if (twctx == NULL) {
      ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
      return -1;
    }

    if (twctx->instruction_filter > 0 &&
        !(twctx->instruction_filter & cur->instruction) ) {
        return 0;
    }

    visitor = (csync_treewalk_visit_func*)(twctx->user_visitor);
    if (visitor != NULL) {
      rc = (*visitor)(cur, other, twctx->userdata);

      return rc;
    }
    ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
    return -1;
}

/*
 * treewalk function, called from its wrappers below.
 *
 * it encapsulates the user visitor function, the filter and the userdata
 * into a treewalk_context structure and calls the rb treewalk function,
 * which calls the local _csync_treewalk_visitor in this module.
 * The user visitor is called from there.
 */
static int _csync_walk_tree(CSYNC *ctx, c_rbtree_t *tree, csync_treewalk_visit_func *visitor, int filter)
{
    _csync_treewalk_context tw_ctx;
    int rc = -1;

    if (ctx == NULL) {
        errno = EBADF;
        return rc;
    }

    if (visitor == NULL || tree == NULL) {
        ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
        return rc;
    }
    
    tw_ctx.userdata = ctx->callbacks.userdata;
    tw_ctx.user_visitor = visitor;
    tw_ctx.instruction_filter = filter;

    ctx->callbacks.userdata = &tw_ctx;

    rc = c_rbtree_walk(tree, (void*) ctx, _csync_treewalk_visitor);
    if( rc < 0 ) {
      if( ctx->status_code == CSYNC_STATUS_OK )
          ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_TREE_ERROR);
    }
    ctx->callbacks.userdata = tw_ctx.userdata;

    return rc;
}

/*
 * wrapper function for treewalk on the remote tree
 */
int csync_walk_remote_tree(CSYNC *ctx,  csync_treewalk_visit_func *visitor, int filter)
{
    c_rbtree_t *tree = NULL;
    int rc = -1;

    if(ctx != NULL) {
        ctx->status_code = CSYNC_STATUS_OK;
        ctx->current = REMOTE_REPLICA;
        tree = ctx->remote.tree;
    }

    /* all error handling in the called function */
    rc = _csync_walk_tree(ctx, tree, visitor, filter);
    return rc;
}

/*
 * wrapper function for treewalk on the local tree
 */
int csync_walk_local_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter)
{
    c_rbtree_t *tree = NULL;
    int rc = -1;

    if (ctx != NULL) {
        ctx->status_code = CSYNC_STATUS_OK;
        ctx->current = LOCAL_REPLICA;
        tree = ctx->local.tree;
    }

    /* all error handling in the called function */
    rc = _csync_walk_tree(ctx, tree, visitor, filter);
    return rc;  
}

static void _tree_destructor(void *data) {
  csync_file_stat_t *freedata = NULL;

  freedata = (csync_file_stat_t *) data;
  delete freedata;
}

/* reset all the list to empty.
 * used by csync_commit and csync_destroy */
static void _csync_clean_ctx(CSYNC *ctx)
{
    /* destroy the rbtrees */
    if (c_rbtree_size(ctx->local.tree) > 0) {
        c_rbtree_destroy(ctx->local.tree, _tree_destructor);
    }

    if (c_rbtree_size(ctx->remote.tree) > 0) {
        c_rbtree_destroy(ctx->remote.tree, _tree_destructor);
    }

    csync_rename_destroy(ctx);

    /* free memory */
    c_rbtree_free(ctx->local.tree);
    c_rbtree_free(ctx->remote.tree);

    SAFE_FREE(ctx->remote.root_perms);
}

int csync_commit(CSYNC *ctx) {
  int rc = 0;

  if (ctx == NULL) {
    return -1;
  }

  ctx->status_code = CSYNC_STATUS_OK;

  if (ctx->statedb.db != NULL
      && csync_statedb_close(ctx) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "ERR: closing of statedb failed.");
    rc = -1;
  }
  ctx->statedb.db = NULL;

  _csync_clean_ctx(ctx);

  ctx->remote.read_from_db = 0;
  ctx->read_remote_from_db = true;
  ctx->db_is_empty = false;


  /* Create new trees */
  c_rbtree_create(&ctx->local.tree, _key_cmp, _data_cmp);
  c_rbtree_create(&ctx->remote.tree, _key_cmp, _data_cmp);


  ctx->status = CSYNC_STATUS_INIT;
  SAFE_FREE(ctx->error_string);

  rc = 0;
  return rc;
}

int csync_destroy(CSYNC *ctx) {
  int rc = 0;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }
  ctx->status_code = CSYNC_STATUS_OK;

  if (ctx->statedb.db != NULL
      && csync_statedb_close(ctx) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "ERR: closing of statedb failed.");
    rc = -1;
  }
  ctx->statedb.db = NULL;

  _csync_clean_ctx(ctx);

  SAFE_FREE(ctx->statedb.file);
  SAFE_FREE(ctx->local.uri);
  SAFE_FREE(ctx->error_string);

  SAFE_FREE(ctx);

  return rc;
}

void *csync_get_userdata(CSYNC *ctx) {
  if (ctx == NULL) {
    return NULL;
  }
  return ctx->callbacks.userdata;
}

int csync_set_userdata(CSYNC *ctx, void *userdata) {
  if (ctx == NULL) {
    return -1;
  }

  ctx->callbacks.userdata = userdata;

  return 0;
}

csync_auth_callback csync_get_auth_callback(CSYNC *ctx) {
  if (ctx == NULL) {
    return NULL;
  }

  return ctx->callbacks.auth_function;
}

int csync_set_status(CSYNC *ctx, int status) {
  if (ctx == NULL || status < 0) {
    return -1;
  }

  ctx->status = status;

  return 0;
}

CSYNC_STATUS csync_get_status(CSYNC *ctx) {
  if (ctx == NULL) {
    return CSYNC_STATUS_ERROR;
  }

  return ctx->status_code;
}

const char *csync_get_status_string(CSYNC *ctx)
{
  return csync_vio_get_status_string(ctx);
}

void csync_request_abort(CSYNC *ctx)
{
  if (ctx != NULL) {
    ctx->abort = true;
  }
}

void csync_resume(CSYNC *ctx)
{
  if (ctx != NULL) {
    ctx->abort = false;
  }
}

int  csync_abort_requested(CSYNC *ctx)
{
  if (ctx != NULL) {
    return ctx->abort;
  } else {
    return (1 == 0);
  }
}
