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

#include <assert.h>
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

/* Returns true if we're reasonably certain that hash equality
 * for the header means content equality.
 *
 * Cryptographic safety is not required - this is mainly
 * intended to rule out checksums like Adler32 that are not intended for
 * hashing and have high likelihood of collision with particular inputs.
 */
static bool _csync_is_collision_safe_hash(const char *checksum_header)
{
    return strncmp(checksum_header, "SHA1:", 5) == 0
        || strncmp(checksum_header, "MD5:", 4) == 0;
}

/**
 * The main function in the reconcile pass.
 *
 * It's called for each entry in the local and remote rbtrees by
 * csync_reconcile()
 *
 * Before the reconcile phase the trees already know about changes
 * relative to the sync journal. This function's job is to spot conflicts
 * between local and remote changes and adjust the nodes accordingly.
 *
 * See doc/dev/sync-algorithm.md for an overview.
 *
 *
 * Older detail comment:
 *
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
        case CSYNC_INSTRUCTION_UPDATE_METADATA:
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
                } else if (other->instruction == CSYNC_INSTRUCTION_NONE
                           || other->instruction == CSYNC_INSTRUCTION_UPDATE_METADATA
                           || cur->type == CSYNC_FTW_TYPE_DIR) {
                    other->instruction = CSYNC_INSTRUCTION_RENAME;
                    other->destpath = c_strdup( cur->path );
                    if( !c_streq(cur->file_id, "") ) {
                        csync_vio_set_file_id( other->file_id, cur->file_id );
                    }
                    other->inode = cur->inode;
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                } else if (other->instruction == CSYNC_INSTRUCTION_REMOVE) {
                    other->instruction = CSYNC_INSTRUCTION_RENAME;
                    other->destpath = c_strdup( cur->path );

                    if( !c_streq(cur->file_id, "") ) {
                        csync_vio_set_file_id( other->file_id, cur->file_id );
                    }
                    other->inode = cur->inode;
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                } else if (other->instruction == CSYNC_INSTRUCTION_NEW) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "OOOO=> NEW detected in other tree!");
                    cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
                } else {
                    assert(other->type != CSYNC_FTW_TYPE_DIR);
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
        bool is_conflict = true;
        /*
     * file found on the other replica
     */
        other = (csync_file_stat_t *) node->data;

        switch (cur->instruction) {
        case CSYNC_INSTRUCTION_UPDATE_METADATA:
            if (other->instruction == CSYNC_INSTRUCTION_UPDATE_METADATA && ctx->current == LOCAL_REPLICA) {
                // Remote wins, the SyncEngine will pick relevant local metadata since the remote tree is walked last.
                cur->instruction = CSYNC_INSTRUCTION_NONE;
            }
            break;
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
                    // Folders of the same path are always considered equals
                    is_conflict = false;
                } else {
                    // If the size or mtime is different, it's definitely a conflict.
                    is_conflict = ((other->size != cur->size) || (other->modtime != cur->modtime));

                    // It could be a conflict even if size and mtime match!
                    //
                    // In older client versions we always treated these cases as a
                    // non-conflict. This behavior is preserved in case the server
                    // doesn't provide a suitable content hash.
                    //
                    // When it does have one, however, we do create a job, but the job
                    // will compare hashes and avoid the download if they are equal.
                    const char *remoteChecksumHeader =
                        (ctx->current == REMOTE_REPLICA ? cur->checksumHeader : other->checksumHeader);
                    if (remoteChecksumHeader) {
                        is_conflict |= _csync_is_collision_safe_hash(remoteChecksumHeader);
                    }

                    // SO: If there is no checksum, we can have !is_conflict here
                    // even though the files have different content! This is an
                    // intentional tradeoff. Downloading and comparing files would
                    // be technically correct in this situation but leads to too
                    // much waste.
                    // In particular this kind of NEW/NEW situation with identical
                    // sizes and mtimes pops up when the local database is lost for
                    // whatever reason.
                }
                if (ctx->current == REMOTE_REPLICA) {
                    // If the files are considered equal, only update the DB with the etag from remote
                    cur->instruction = is_conflict ? CSYNC_INSTRUCTION_CONFLICT : CSYNC_INSTRUCTION_UPDATE_METADATA;
                    other->instruction = CSYNC_INSTRUCTION_NONE;
                } else {
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                    other->instruction = is_conflict ? CSYNC_INSTRUCTION_CONFLICT : CSYNC_INSTRUCTION_UPDATE_METADATA;
                }

                break;
                /* file on the other replica has not been modified */
            case CSYNC_INSTRUCTION_NONE:
            case CSYNC_INSTRUCTION_UPDATE_METADATA:
                if (cur->type != other->type) {
                    // If the type of the entity changed, it's like NEW, but
                    // needs to delete the other entity first.
                    cur->instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
                    other->instruction = CSYNC_INSTRUCTION_NONE;
                } else if (cur->type == CSYNC_FTW_TYPE_DIR) {
                    cur->instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                    other->instruction = CSYNC_INSTRUCTION_NONE;
                } else {
                    cur->instruction = CSYNC_INSTRUCTION_SYNC;
                    other->instruction = CSYNC_INSTRUCTION_NONE;
                }
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
    const char *repo = ctx->current == REMOTE_REPLICA ? "server" : "client";
    if(cur->instruction ==CSYNC_INSTRUCTION_NONE)
    {
        if(cur->type == CSYNC_FTW_TYPE_DIR)
        {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
                      "%-30s %s dir:  %s",
                      csync_instruction_str(cur->instruction),
                      repo,
                      cur->path);
        }
        else
        {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
                      "%-30s %s file: %s",
                      csync_instruction_str(cur->instruction),
                      repo,
                      cur->path);
        }
    }
    else
    {
        if(cur->type == CSYNC_FTW_TYPE_DIR)
        {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO,
                      "%-30s %s dir:  %s",
                      csync_instruction_str(cur->instruction),
                      repo,
                      cur->path);
        }
        else
        {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO,
                      "%-30s %s file: %s",
                      csync_instruction_str(cur->instruction),
                      repo,
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
