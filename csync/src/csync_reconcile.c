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

#include "config_csync.h"

#include "csync_private.h"
#include "csync_reconcile.h"
#include "csync_util.h"
#include "csync_statedb.h"
#include "csync_rename.h"
#include "c_jhash.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.reconciler"
#include "csync_log.h"

#include "inttypes.h"

/* Check if a file is ignored because one parent is ignored.
 * return the node of the ignored directoy if it's the case, or NULL if it is not ignored */
static c_rbnode_t *_csync_check_ignored(c_rbtree_t *tree, const char *path, int pathlen) {
    uint64_t h = 0;
    c_rbnode_t *node = NULL;

    /* compute the size of the parent directory */
    int parentlen = pathlen - 1;
    while (parentlen > 0 && path[parentlen] != '/') {
        parentlen--;
    }
    if (parentlen <= 0) {
        return NULL;
    }

    h = c_jhash64((uint8_t *) path, parentlen, 0);
    node = c_rbtree_find(tree, &h);
    if (node) {
        csync_file_stat_t *n = (csync_file_stat_t*)node->data;
        if (n->instruction == CSYNC_INSTRUCTION_IGNORE) {
            /* Yes, we are ignored */
            return node;
        } else {
            /* Not ignored */
            return NULL;
        }
    } else {
        /* Try if the parent itself is ignored */
        return _csync_check_ignored(tree, path, parentlen);
    }
}

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
    csync_file_stat_t *tmp = NULL;
    uint64_t h = 0;
    int len = 0;

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

    if (!node) {
        /* Check the renamed path as well. */
        char *renamed_path = csync_rename_adjust_path(ctx, cur->path);
        if (!c_streq(renamed_path, cur->path)) {
            len = strlen( renamed_path );
            h = c_jhash64((uint8_t *) renamed_path, len, 0);
            node = c_rbtree_find(tree, &h);
        }
        SAFE_FREE(renamed_path);
    }
    if (!node) {
        /* Check if it is ignored */
        node = _csync_check_ignored(tree, cur->path, cur->pathlen);
        /* If it is ignored, other->instruction will be  IGNORE so this one will also be ignored */
    }

    /* file only found on current replica */
    if (node == NULL) {
        switch(cur->instruction) {
        /* file has been modified */
        case CSYNC_INSTRUCTION_EVAL:
            cur->instruction = CSYNC_INSTRUCTION_NEW;
            break;
            /* file has been removed on the opposite replica */
        case CSYNC_INSTRUCTION_NONE:
            if (cur->has_ignored_files) {
                /* Do not remove a directory that has ignored files */
                break;
            }
            if (cur->child_modified) {
                /* re-create directory that has modified contents */
                cur->instruction = CSYNC_INSTRUCTION_NEW;
                break;
            }
            cur->instruction = CSYNC_INSTRUCTION_REMOVE;
            break;
        case CSYNC_INSTRUCTION_EVAL_RENAME:
            if(ctx->current == LOCAL_REPLICA ) {
                /* use the old name to find the "other" node */
                tmp = csync_statedb_get_stat_by_inode(ctx, cur->inode);
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Finding opposite temp through inode %" PRIu64 ": %s",
                          cur->inode, tmp ? "true":"false");
            } else if( ctx->current == REMOTE_REPLICA ) {
                tmp = csync_statedb_get_stat_by_file_id(ctx, cur->file_id);
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Finding opposite temp through file ID %s: %s",
                          cur->file_id, tmp ? "true":"false");
            } else {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Unknown replica...");
            }

            if( tmp ) {
                len = strlen( tmp->path );
                if( len > 0 ) {
                    h = c_jhash64((uint8_t *) tmp->path, len, 0);
                    /* First, check that the file is NOT in our tree (another file with the same name was added) */
                    node = c_rbtree_find(ctx->current == REMOTE_REPLICA ? ctx->remote.tree : ctx->local.tree, &h);
                    if (node) {
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Origin found in our tree : %s", tmp->path);
                    } else {
                        /* Find the temporar file in the other tree. */
                        node = c_rbtree_find(tree, &h);
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "PHash of temporary opposite (%s): %" PRIu64 " %s",
                                tmp->path , h, node ? "found": "not found" );
                        if (node) {
                            other = (csync_file_stat_t*)node->data;
                        } else {
                            /* the renamed file could not be found in the opposite tree. That is because it
                            * is not longer existing there, maybe because it was renamed or deleted.
                            * The journal is cleaned up later after propagation.
                            */
                        }
                    }
                }

                if(!other) {
                    cur->instruction = CSYNC_INSTRUCTION_NEW;
                    if (cur->type == CSYNC_FTW_TYPE_DIR) {
                        // For new directories we always want to update the etag once
                        // the directory has been propagated. Otherwise the directory
                        // could appear locally without being added to the database.
                        cur->should_update_metadata = true;
                    }
                } else if (other->instruction == CSYNC_INSTRUCTION_NONE
                           || cur->type == CSYNC_FTW_TYPE_DIR) {
                    other->instruction = CSYNC_INSTRUCTION_RENAME;
                    other->destpath = c_strdup( cur->path );
                    if( !c_streq(cur->file_id, "") ) {
                        csync_vio_set_file_id( other->file_id, cur->file_id );
                    }
                    other->inode = cur->inode;
                    other->should_update_metadata = true;
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                } else if (other->instruction == CSYNC_INSTRUCTION_REMOVE) {
                    other->instruction = CSYNC_INSTRUCTION_RENAME;
                    other->destpath = c_strdup( cur->path );

                    if( !c_streq(cur->file_id, "") ) {
                        csync_vio_set_file_id( other->file_id, cur->file_id );
                    }
                    other->inode = cur->inode;
                    other->should_update_metadata = true;
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                } else if (other->instruction == CSYNC_INSTRUCTION_NEW) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "OOOO=> NEW detected in other tree!");
                    cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
                } else {
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                    other->instruction = CSYNC_INSTRUCTION_SYNC;
                }
                csync_file_stat_free(tmp);
           }

            break;
        default:
            break;
        }
    } else {
        bool is_equal_files = false;
        /*
     * file found on the other replica
     */
        other = (csync_file_stat_t *) node->data;

        switch (cur->instruction) {
        case CSYNC_INSTRUCTION_EVAL_RENAME:
            /* If the file already exist on the other side, we have a conflict.
               Abort the rename and consider it is a new file. */
            cur->instruction = CSYNC_INSTRUCTION_NEW;
            /* fall trough */
        /* file on current replica is changed or new */
        case CSYNC_INSTRUCTION_EVAL:
        case CSYNC_INSTRUCTION_NEW:
            // This operation is usually a no-op and will by default return false
            if (csync_file_locked_or_open(ctx->local.uri, cur->path)) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "[Reconciler] IGNORING file %s/%s since it is locked / open", ctx->local.uri, cur->path);
                cur->instruction = CSYNC_INSTRUCTION_ERROR;
                if (cur->error_status == CSYNC_STATUS_OK) // don't overwrite error
                    cur->error_status = CYSNC_STATUS_FILE_LOCKED_OR_OPEN;
                break;
            } else {
                //CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "[Reconciler] not ignoring file %s/%s", ctx->local.uri, cur->path);
            }
            switch (other->instruction) {
            /* file on other replica is changed or new */
            case CSYNC_INSTRUCTION_NEW:
            case CSYNC_INSTRUCTION_EVAL:
                if (other->type == CSYNC_FTW_TYPE_DIR &&
                        cur->type == CSYNC_FTW_TYPE_DIR) {
                    is_equal_files = (other->modtime == cur->modtime);
                } else {
                    is_equal_files = ((other->size == cur->size) && (other->modtime == cur->modtime));
                    // FIXME: do a binary comparision of the file here because of the following
                    // edge case:
                    // The files could still have different content, even though the mtime
                    // and size are the same.
                }
                if (is_equal_files) {
                    /* The files are considered equal. */
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                    other->instruction = CSYNC_INSTRUCTION_NONE;

                    /* update DB with new etag from remote */
                    if (ctx->current == LOCAL_REPLICA) {
                        other->should_update_metadata = true;
                    } else {
                        cur->should_update_metadata = true;
                    }
                } else if(ctx->current == REMOTE_REPLICA) {
                        cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
                        other->instruction = CSYNC_INSTRUCTION_NONE;
                } else {
                        cur->instruction = CSYNC_INSTRUCTION_NONE;
                        other->instruction = CSYNC_INSTRUCTION_CONFLICT;
                }

                break;
                /* file on the other replica has not been modified */
            case CSYNC_INSTRUCTION_NONE:
                cur->instruction = CSYNC_INSTRUCTION_SYNC;
                break;
            case CSYNC_INSTRUCTION_IGNORE:
                cur->instruction = CSYNC_INSTRUCTION_IGNORE;
            break;
            default:
                break;
            }
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
