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
 */

#include "config.h"

#include "csync_private.h"
#include "csync_reconcile.h"
#include "csync_util.h"
#include "csync_statedb.h"
#include "csync_rename.h"
#include "c_jhash.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.reconciler"
#include "csync_log.h"

#define ACCEPTED_TIME_DIFF 5
#define ONE_HOUR 3600

static bool _time_dst_off( time_t t1, time_t t2, int dst_offset ) {
    bool ret = false;
    long int diff = t1 - t2;
    if( diff > (dst_offset - ACCEPTED_TIME_DIFF) && (diff < dst_offset+ ACCEPTED_TIME_DIFF) )
        ret = true;

    return ret;
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

    if (!node && ctx->current == REMOTE_REPLICA) {
        /* Check the renamed path as well. */
        char *renamed_path = csync_rename_adjust_path(ctx, cur->path);
        if (!c_streq(renamed_path, cur->path)) {
            len = strlen( renamed_path );
            h = c_jhash64((uint8_t *) renamed_path, len, 0);
            node = c_rbtree_find(tree, &h);
        }
        SAFE_FREE(renamed_path);
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
            cur->instruction = CSYNC_INSTRUCTION_REMOVE;
            break;
        case CSYNC_INSTRUCTION_RENAME:
            /* rename support only on the local replica because of inode needed. */
            if(ctx->current == LOCAL_REPLICA ) {
                /* use the old name to find the "other" node */
                tmp = csync_statedb_get_stat_by_inode(ctx, cur->inode);
                /* Find the opposite node. */
                if( tmp ) {
                    /* We need to calculate the phash again because of the phash being stored as int in db. */
                    if( tmp->path ) {
                        len = strlen( tmp->path );
                        h = c_jhash64((uint8_t *) tmp->path, len, 0);

                        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"PHash of temporar opposite: %llu", h);
                        node = c_rbtree_find(tree, &h);
                    }
                    if(node) {
                        char *adjusted = csync_rename_adjust_path(ctx, cur->path);
                        if (!c_streq(adjusted, cur->path)) {
                            other = (csync_file_stat_t*)node->data;
                            other->instruction = CSYNC_INSTRUCTION_RENAME;
                            other->destpath = c_strdup( cur->path );
                            cur->instruction = CSYNC_INSTRUCTION_NONE;
                        } else {
                            /* The parent directory is going to be renamed */
                            cur->instruction = CSYNC_INSTRUCTION_NONE;
                        }
                        SAFE_FREE(adjusted);
                    }
                    if( ! other ) {
                        cur->instruction = CSYNC_INSTRUCTION_NEW;
                    }
                    SAFE_FREE(tmp->md5);
                    SAFE_FREE(tmp);
                }
            }
            break;
        default:
            break;
        }
    } else {
        /*
     * file found on the other replica
     */
        bool set_instruction_none = false;
        other = (csync_file_stat_t *) node->data;

        switch (cur->instruction) {
        case CSYNC_INSTRUCTION_RENAME:
            /* If the file already exist on the other side, we have a conflict.
               Abort the rename and consider it is a new file. */
            cur->instruction = CSYNC_INSTRUCTION_NEW;
            /* fall trough */
        /* file on current replica is new */
        case CSYNC_INSTRUCTION_NEW:
            switch (other->instruction) {
            /* file on other replica is new too */
            case CSYNC_INSTRUCTION_NEW:
                /* if (cur->modtime > other->modtime) { */
                CSYNC_LOG( CSYNC_LOG_PRIORITY_DEBUG, "** size compare: %lld <-> %lld", (long long) cur->size,
                           (long long) other->size);
                if(cur->modtime - other->modtime > ACCEPTED_TIME_DIFF) {
                    if( other->size == cur->size &&
                            _time_dst_off( cur->modtime, other->modtime, ONE_HOUR ) ) {
                        CSYNC_LOG( CSYNC_LOG_PRIORITY_DEBUG, "DST-Problem detected. Skip conflict for %s!", cur->path);
                        set_instruction_none = true;
                    } else {
                        if(ctx->options.with_conflict_copys)
                        {
                            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"file new on both, cur is newer PATH=./%s, %llu <-> %llu",
                                      cur->path, (unsigned long long) cur->modtime, (unsigned long long) other->modtime);
                            cur->instruction = CSYNC_INSTRUCTION_CONFLICT;
                            other->instruction = CSYNC_INSTRUCTION_NONE;
                        }
                        else
                        {
                            cur->instruction = CSYNC_INSTRUCTION_SYNC;
                            other->instruction = CSYNC_INSTRUCTION_NONE;
                        }
                    }

                    /* } else if (cur->modtime < other->modtime) { */
                } else if (other->modtime - cur->modtime > ACCEPTED_TIME_DIFF) {
                    /* Check if we have the dst problem. Older versions of ocsync wrote a wrong
                     * (ie. localized) mtime to the files which can be ignored if the size is equal
                     * and the time shift is exactyl one hour. */
                    if( other->size == cur->size &&
                            _time_dst_off( other->modtime, cur->modtime, ONE_HOUR ) ) {
                        CSYNC_LOG( CSYNC_LOG_PRIORITY_DEBUG, "DST-Problem detected. Skip conflict for %s!", cur->path);
                        set_instruction_none = true;
                    } else {
                        if(ctx->options.with_conflict_copys)
                        {
                            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"file new on both, other is newer PATH=./%s, %llu <->%llu",
                                      cur->path, (unsigned long long) cur->modtime, (unsigned long long) other->modtime);
                            cur->instruction = CSYNC_INSTRUCTION_NONE;
                            other->instruction = CSYNC_INSTRUCTION_CONFLICT;
                        }
                        else
                        {
                            cur->instruction = CSYNC_INSTRUCTION_NONE;
                            other->instruction = CSYNC_INSTRUCTION_SYNC;
                        }
                    }

                } else {
                    /* The files are equal. */
                    set_instruction_none = true;
                }

                if( set_instruction_none ) {
                    /* file are equal */
                    /* FIXME: Get the id from the server! */
                    cur->instruction = CSYNC_INSTRUCTION_NONE;
                    other->instruction = CSYNC_INSTRUCTION_NONE;

                    if( !cur->md5 && other->md5 ) cur->md5 = c_strdup(other->md5);
                }

                break;
                /* file on other replica has changed too */
            case CSYNC_INSTRUCTION_EVAL:
                /* file on current replica is newer */
                if (cur->modtime - other->modtime > ACCEPTED_TIME_DIFF ) {

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
                if (cur->modtime - other->modtime > ACCEPTED_TIME_DIFF) {

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
                if (cur->modtime - other->modtime > ACCEPTED_TIME_DIFF) {

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

  return rc;
}

/* vim: set ts=8 sw=2 et cindent: */
