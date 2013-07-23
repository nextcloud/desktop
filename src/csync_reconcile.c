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

#include "config.h"

#include "csync_private.h"
#include "csync_reconcile.h"
#include "csync_util.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.reconciler"
#include "csync_log.h"

/*
 * We merge replicas at the file level. The merged replica contains the
 * superset of files that are on the local machine and server copies of
 * the replica. In the case where the same file is in both the local
 * and server copy, the file that was modified most recently is used.
 * This means that new files are not deleted, and updated versions of
 * existing files are not overwritten.
 *
 * When a file is updated, the merge algorithm compares the destination
 * file with the the source file. If the destination file is newer
 * (timestamp is newer), it is not overwritten. If both files, on the
 * source and the destination, have been changed, the newer file wins.
 */
static int _csync_merge_algorithm_visitor(void *obj, void *data) {
  csync_file_stat_t *cur = NULL;
  csync_file_stat_t *other = NULL;
  CSYNC *ctx = NULL;
  c_rbtree_t *tree = NULL;
  c_rbnode_t *node = NULL;

  cur = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  /* we need the opposite tree! */
  switch (ctx->current) {
    case LOCAL_REPLICA:
      tree = ctx->remote.tree;
      break;
    case REMOTE_REPLICA:
      tree = ctx->local.tree;
      break;
    default:
      break;
  }

  node = c_rbtree_find(tree, &cur->phash);
  /* file only found on current replica */
  if (node == NULL) {
    switch(cur->instruction) {
      /* file has been modified */
      case CSYNC_INSTRUCTION_EVAL:
        cur->instruction = CSYNC_INSTRUCTION_NEW;
        break;
      /* file has been removed on the opposite replica */
      case CSYNC_INSTRUCTION_NONE:
        cur->instruction = CSYNC_INSTRUCTION_REMOVE;
        break;
      case CSYNC_INSTRUCTION_RENAME:
        cur->instruction = CSYNC_INSTRUCTION_NEW;
        break;
      default:
        break;
    }
  } else {
    /*
     * file found on the other replica
     */
    other = (csync_file_stat_t *) node->data;

    switch (cur->instruction) {
      /* file on current replica is new */
      case CSYNC_INSTRUCTION_NEW:
        switch (other->instruction) {
          /* file on other replica is new too */
          case CSYNC_INSTRUCTION_NEW:
            if (cur->modtime > other->modtime) {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"file new on both, cur is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
				other->instruction = CSYNC_INSTRUCTION_NONE;
			  }
			  else
			  {
				cur->instruction = CSYNC_INSTRUCTION_SYNC;
				other->instruction = CSYNC_INSTRUCTION_NONE;
			  }
			  
            } else if (cur->modtime < other->modtime) {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"file new on both, other is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_NONE;
				other->instruction = CSYNC_INSTRUCTION_CONFLICT;
			  }
			  else
			  {
				cur->instruction = CSYNC_INSTRUCTION_NONE;
				other->instruction = CSYNC_INSTRUCTION_SYNC;
			  }
			  
            } else {
              /* file are equal */
              cur->instruction = CSYNC_INSTRUCTION_NONE;
              other->instruction = CSYNC_INSTRUCTION_NONE;
            }
            break;
          /* file on other replica has changed too */
          case CSYNC_INSTRUCTION_EVAL:
            /* file on current replica is newer */
            if (cur->modtime > other->modtime) {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"new on cur, modified on other, cur is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
			  }
			  else
			  {
				cur->instruction = CSYNC_INSTRUCTION_SYNC;
			  }
			  
            } else {
              /* file on opposite replica is newer */
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"new on cur, modified on other, other is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_NONE;
			  }
			  else
			  {
				  cur->instruction = CSYNC_INSTRUCTION_NONE;
			  }
			  
            }
            break;
          /* file on the other replica has not been modified */
          case CSYNC_INSTRUCTION_NONE:
            cur->instruction = CSYNC_INSTRUCTION_SYNC;
            break;
          default:
            break;
        }
        break;
      /* file on current replica has been modified */
      case CSYNC_INSTRUCTION_EVAL:
        switch (other->instruction) {
          /* file on other replica is new too */
          case CSYNC_INSTRUCTION_NEW:
            if (cur->modtime > other->modtime) {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"modified on cur, new on other, cur is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
			  }
			  else
			  {
				  cur->instruction = CSYNC_INSTRUCTION_SYNC;
			  }
			  
            } else {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"modified on cur, new on other, other is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_NONE;
			  }
			  else
			  {
				  cur->instruction = CSYNC_INSTRUCTION_NONE;
			  }
            }
            break;
          /* file on other replica has changed too */
          case CSYNC_INSTRUCTION_EVAL:
            /* file on current replica is newer */
            if (cur->modtime > other->modtime) {
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"both modified, cur is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
				other->instruction= CSYNC_INSTRUCTION_NONE;
			  }
			  else
			  {
				cur->instruction = CSYNC_INSTRUCTION_SYNC;
			  }
			  
            } else {
              /* file on opposite replica is newer */
              
			  if(ctx->options.with_conflict_copys)
			  {
				CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"both modified, other is newer PATH=./%s",cur->path);
				cur->instruction = CSYNC_INSTRUCTION_NONE;
				other->instruction=CSYNC_INSTRUCTION_CONFLICT;
			  }
			  else
			  {
				cur->instruction = CSYNC_INSTRUCTION_NONE;
			  }
            }
            break;
          /* file on the other replica has not been modified */
          case CSYNC_INSTRUCTION_NONE:
            cur->instruction = CSYNC_INSTRUCTION_SYNC;
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
  
  //hide instruction NONE messages when log level is set to debug, 
  //only show these messages on log level trace 
  if(cur->instruction ==CSYNC_INSTRUCTION_NONE)
  {
      if(cur->type == CSYNC_FTW_TYPE_DIR)
      {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
        "%-20s  dir: %s",
        csync_instruction_str(cur->instruction),
        cur->path);
      }
      else
      {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
        "%-20s file: %s",
        csync_instruction_str(cur->instruction),
        cur->path);   
      }
  }
  else
  {
      if(cur->type == CSYNC_FTW_TYPE_DIR)
      {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
        "%-20s  dir: %s",
        csync_instruction_str(cur->instruction),
        cur->path);
      }
      else
      {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
        "%-20s file: %s",
        csync_instruction_str(cur->instruction),
        cur->path);   
      }
  }
  
  return 0;
}

int csync_reconcile_updates(CSYNC *ctx) {
  int rc;
  c_rbtree_t *tree = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      tree = ctx->local.tree;
      break;
    case REMOTE_REPLICA:
      tree = ctx->remote.tree;
      break;
    default:
      break;
  }

  rc = c_rbtree_walk(tree, (void *) ctx, _csync_merge_algorithm_visitor);
  if( rc < 0 ) {
    ctx->status_code = CSYNC_STATUS_RECONCILE_ERROR;
  }
  return rc;
}

/* vim: set ts=8 sw=2 et cindent: */
