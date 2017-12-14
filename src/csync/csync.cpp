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
#include "csync_time.h"
#include "csync_util.h"
#include "csync_misc.h"
#include "std/c_private.h"

#include "csync_update.h"
#include "csync_reconcile.h"

#include "vio/csync_vio.h"

#include "csync_log.h"
#include "csync_rename.h"
#include "common/c_jhash.h"
#include "common/syncjournalfilerecord.h"


csync_s::csync_s(const char *localUri, OCC::SyncJournalDb *statedb)
  : statedb(statedb)
{
  size_t len = 0;

  /* remove trailing slashes */
  len = strlen(localUri);
  while(len > 0 && localUri[len - 1] == '/') --len;

  local.uri = c_strndup(localUri, len);
}

int csync_update(CSYNC *ctx) {
  int rc = -1;
  struct timespec start, finish;

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }
  ctx->status_code = CSYNC_STATUS_OK;

  ctx->status_code = CSYNC_STATUS_OK;

  csync_memstat_check();

  if (!ctx->exclude_traversal_fn) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "No exclude file loaded or defined!");
  }

  /* update detection for local replica */
  csync_gettime(&start);
  ctx->current = LOCAL_REPLICA;

  rc = csync_ftw(ctx, ctx->local.uri, csync_walker, MAX_DEPTH);
  if (rc < 0) {
    if(ctx->status_code == CSYNC_STATUS_OK) {
        ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
    }
    return rc;
  }

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for local replica took %.2f seconds walking %zu files.",
            c_secdiff(finish, start), ctx->local.files.size());
  csync_memstat_check();

  /* update detection for remote replica */
  csync_gettime(&start);
  ctx->current = REMOTE_REPLICA;

  rc = csync_ftw(ctx, "", csync_walker, MAX_DEPTH);
  if (rc < 0) {
      if(ctx->status_code == CSYNC_STATUS_OK) {
          ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
      }
      return rc;
  }

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Update detection for remote replica took %.2f seconds "
            "walking %zu files.",
            c_secdiff(finish, start), ctx->remote.files.size());
  csync_memstat_check();

  ctx->status |= CSYNC_STATUS_UPDATE;

  rc = 0;
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

  ctx->current = LOCAL_REPLICA;

  rc = csync_reconcile_updates(ctx);

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
      "Reconciliation for local replica took %.2f seconds visiting %zu files.",
      c_secdiff(finish, start), ctx->local.files.size());

  if (rc < 0) {
      if (!CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = csync_errno_to_status( errno, CSYNC_STATUS_RECONCILE_ERROR );
      }
      return rc;
  }

  /* Reconciliation for remote replica */
  csync_gettime(&start);

  ctx->current = REMOTE_REPLICA;

  rc = csync_reconcile_updates(ctx);

  csync_gettime(&finish);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
      "Reconciliation for remote replica took %.2f seconds visiting %zu files.",
      c_secdiff(finish, start), ctx->remote.files.size());

  if (rc < 0) {
      if (!CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = csync_errno_to_status(errno,  CSYNC_STATUS_RECONCILE_ERROR );
      }
      return rc;
  }

  ctx->status |= CSYNC_STATUS_RECONCILE;

  rc = 0;
  return rc;
}

/*
 * local visitor which calls the user visitor with repacked stat info.
 */
static int _csync_treewalk_visitor(csync_file_stat_t *cur, CSYNC * ctx) {
    int rc = 0;
    csync_treewalk_visit_func *visitor   = NULL;
    _csync_treewalk_context *twctx = NULL;
    csync_s::FileMap *other_tree = nullptr;

    if (ctx == NULL) {
      return -1;
    }

    /* we need the opposite tree! */
    switch (ctx->current) {
    case LOCAL_REPLICA:
        other_tree = &ctx->remote.files;
        break;
    case REMOTE_REPLICA:
        other_tree = &ctx->local.files;
        break;
    default:
        break;
    }

    csync_s::FileMap::const_iterator other_file_it = other_tree->find(cur->path);

    if (other_file_it == other_tree->cend()) {
        /* Check the renamed path as well. */
        QByteArray renamed_path = csync_rename_adjust_parent_path(ctx, cur->path);
        if (renamed_path != cur->path)
            other_file_it = other_tree->find(renamed_path);
    }

    if (other_file_it == other_tree->cend()) {
        /* Check the source path as well. */
        QByteArray renamed_path = csync_rename_adjust_parent_path_source(ctx, cur->path);
        if (renamed_path != cur->path)
            other_file_it = other_tree->find(renamed_path);
    }

    csync_file_stat_t *other = (other_file_it != other_tree->cend()) ? other_file_it->second.get() : NULL;

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
static int _csync_walk_tree(CSYNC *ctx, csync_s::FileMap *tree, csync_treewalk_visit_func *visitor, int filter)
{
    _csync_treewalk_context tw_ctx;
    int rc = 0;

    tw_ctx.userdata = ctx->callbacks.userdata;
    tw_ctx.user_visitor = visitor;
    tw_ctx.instruction_filter = filter;

    ctx->callbacks.userdata = &tw_ctx;

    for (auto &pair : *tree) {
        if (_csync_treewalk_visitor(pair.second.get(), ctx) < 0) {
          rc = -1;
          break;
        }
    }

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
    ctx->status_code = CSYNC_STATUS_OK;
    ctx->current = REMOTE_REPLICA;
    return _csync_walk_tree(ctx, &ctx->remote.files, visitor, filter);
}

/*
 * wrapper function for treewalk on the local tree
 */
int csync_walk_local_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter)
{
    ctx->status_code = CSYNC_STATUS_OK;
    ctx->current = LOCAL_REPLICA;
    return _csync_walk_tree(ctx, &ctx->local.files, visitor, filter);
}

int csync_s::reinitialize() {
  int rc = 0;

  status_code = CSYNC_STATUS_OK;

  remote.read_from_db = 0;
  read_remote_from_db = true;

  local.files.clear();
  remote.files.clear();

  renames.folder_renamed_from.clear();
  renames.folder_renamed_to.clear();

  local_discovery_style = LocalDiscoveryStyle::FilesystemOnly;
  locally_touched_dirs.clear();

  status = CSYNC_STATUS_INIT;
  SAFE_FREE(error_string);

  rc = 0;
  return rc;
}

csync_s::~csync_s() {
  SAFE_FREE(local.uri);
  SAFE_FREE(error_string);
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

std::unique_ptr<csync_file_stat_t> csync_file_stat_s::fromSyncJournalFileRecord(const OCC::SyncJournalFileRecord &rec)
{
    std::unique_ptr<csync_file_stat_t> st(new csync_file_stat_t);
    st->path = rec._path;
    st->inode = rec._inode;
    st->modtime = rec._modtime;
    st->type = static_cast<ItemType>(rec._type);
    st->etag = rec._etag;
    st->file_id = rec._fileId;
    st->remotePerm = rec._remotePerm;
    st->size = rec._fileSize;
    st->has_ignored_files = rec._serverHasIgnoredFiles;
    st->checksumHeader = rec._checksumHeader;
    return st;
}
