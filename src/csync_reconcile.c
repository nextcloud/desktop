/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
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
 * vim: ts=2 sw=2 et cindent
 */

#include "csync_private.h"
#include "csync_reconcile.h"
#include "csync_util.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.reconciler"
#include "csync_log.h"

static int csync_merge_algorithm_visitor(void *obj, void *data) {
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
    case REMOTE_REPLCIA:
      tree = ctx->local.tree;
      break;
    default:
      break;
  }

  node = c_rbtree_find(tree, (void *) cur->phash);
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
              cur->instruction = CSYNC_INSTRUCTION_SYNC;
              other->instruction = CSYNC_INSTRUCTION_NONE;
            } else if (cur->modtime > other->modtime) {
              cur->instruction = CSYNC_INSTRUCTION_NONE;
              other->instruction = CSYNC_INSTRUCTION_SYNC;
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
              cur->instruction = CSYNC_INSTRUCTION_SYNC;
            } else {
              /* file on opposite replica is newer */
              cur->instruction = CSYNC_INSTRUCTION_REMOVE;
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
              cur->instruction = CSYNC_INSTRUCTION_SYNC;
            } else {
              cur->instruction = CSYNC_INSTRUCTION_NONE;
            }
            break;
          /* file on other replica has changed too */
          case CSYNC_INSTRUCTION_EVAL:
            /* file on current replica is newer */
            if (cur->modtime > other->modtime) {
              cur->instruction = CSYNC_INSTRUCTION_SYNC;
            } else {
              /* file on opposite replica is newer */
              cur->instruction = CSYNC_INSTRUCTION_REMOVE;
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

  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s, instruction: %s", cur->path, csync_instruction_str(cur->instruction));
  return 0;
}

int csync_reconcile_updates(CSYNC *ctx) {
  int rc = -1;
  c_rbtree_t *tree = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      tree = ctx->local.tree;
      break;
    case REMOTE_REPLCIA:
      tree = ctx->remote.tree;
      break;
    default:
      break;
  }

  rc = c_rbtree_walk(tree, (void *) ctx, csync_merge_algorithm_visitor);

  return 0;
}

