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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>

#include "c_lib.h"
#include "c_jhash.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_statedb.h"
#include "csync_update.h"
#include "csync_util.h"
#include "csync_misc.h"

#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.updater"
#include "csync_log.h"
#include "csync_rename.h"

/* calculate the hash of a given uri */
static uint64_t _hash_of_file(CSYNC *ctx, const char *file) {
  const char *path;
  int len;
  uint64_t h = 0;

  if( ctx && file ) {
    path = file;
    if (ctx->current == LOCAL_REPLICA) {
      if (strlen(path) <= strlen(ctx->local.uri)) {
        return 0;
      }
      path += strlen(ctx->local.uri) + 1;
    }
    len = strlen(path);
    h = c_jhash64((uint8_t *) path, len, 0);
  }
  return h;
}

#ifdef NO_RENAME_EXTENSION
/* Return true if the two path have the same extension. false otherwise. */
static bool _csync_sameextension(const char *p1, const char *p2) {
    /* Find pointer to the extensions */
    const char *e1 = strrchr(p1, '.');
    const char *e2 = strrchr(p2, '.');

    /* If the found extension contains a '/', it is because the . was in the folder name
     *            => no extensions */
    if (e1 && strchr(e1, '/')) e1 = NULL;
    if (e2 && strchr(e2, '/')) e2 = NULL;

    /* If none have extension, it is the same extension */
    if (!e1 && !e2)
        return true;

    /* c_streq takes care of the rest */
    return c_streq(e1, e2);
}
#endif

static bool _last_db_return_error(CSYNC* ctx) {
    return ctx->statedb.lastReturnValue != SQLITE_OK && ctx->statedb.lastReturnValue != SQLITE_DONE && ctx->statedb.lastReturnValue != SQLITE_ROW;
}

/*
 * This static method is needed because the type members of the two structs use
 * different enum values. A direct comparion is not neccessarily correct.
 *
 * tmp is csync_file_stat_t
 * fs  is csync_vio_file_stat_t with this vio type:
 *  enum csync_vio_file_type_e {
 *              CSYNC_VIO_FILE_TYPE_UNKNOWN,
 *              CSYNC_VIO_FILE_TYPE_REGULAR,
 *              CSYNC_VIO_FILE_TYPE_DIRECTORY,
 *              CSYNC_VIO_FILE_TYPE_FIFO,
 *              CSYNC_VIO_FILE_TYPE_SOCKET,
 *              CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE,
 *              CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE,
 *              CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK
 *            };
 *
 * csync_file_stat_t can be:
 * CSYNC_FTW_TYPE_SKIP, CSYNC_FTW_TYPE_FILE
 * CSYNC_FTW_TYPE_DIR, CSYNC_FTW_TYPE_SLINK
 */
static bool _csync_filetype_different( const csync_file_stat_t *tmp, const csync_vio_file_stat_t *fs)
{
    if( !(tmp && fs)) return false;

    if( tmp->type == CSYNC_FTW_TYPE_SKIP ) return true;

    if( tmp->type == CSYNC_FTW_TYPE_DIR && fs->type != CSYNC_VIO_FILE_TYPE_DIRECTORY )
        return true;
    if( tmp->type == CSYNC_FTW_TYPE_FILE && fs->type != CSYNC_VIO_FILE_TYPE_REGULAR )
        return true;
    if( tmp->type == CSYNC_FTW_TYPE_SLINK && fs->type != CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK )
        return true;

    return false; // both are NOT different.
}

/* Return true if two mtime are considered equal
 * We consider mtime that are one hour difference to be equal if they are one hour appart
 * because on some system (FAT) the date is changing when the daylight saving is changing */
static bool _csync_mtime_equal(time_t a, time_t b)
{
    if (a == b)
        return true;

    /* 1h of difference +- 1 second because the accuracy of FAT is 2 seconds (#2438) */
    if (fabs(3600 - fabs(difftime(a, b))) < 2)
        return true;

    return false;
}

/**
 * The main function of the discovery/update pass.
 *
 * It's called (indirectly) by csync_update(), once for each entity in the
 * local filesystem and once for each entity in the server data.
 *
 * It has two main jobs:
 * - figure out whether anything happened compared to the sync journal
 *   and set (primarily) the instruction flag accordingly
 * - build the ctx->local.tree / ctx->remote.tree
 *
 * See doc/dev/sync-algorithm.md for an overview.
 */
static int _csync_detect_update(CSYNC *ctx, const char *file,
    const csync_vio_file_stat_t *fs, const int type) {
  uint64_t h = 0;
  size_t len = 0;
  size_t size = 0;
  const char *path = NULL;
  csync_file_stat_t *st = NULL;
  csync_file_stat_t *tmp = NULL;
  CSYNC_EXCLUDE_TYPE excluded;

  if ((file == NULL) || (fs == NULL)) {
    errno = EINVAL;
    ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
    return -1;
  }

  path = file;
  if (ctx->current == LOCAL_REPLICA) {
      if (strlen(path) <= strlen(ctx->local.uri)) {
        ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
        return -1;
      }
      path += strlen(ctx->local.uri) + 1;
  }

  len = strlen(path);

  if (type == CSYNC_FTW_TYPE_SKIP) {
      excluded =CSYNC_FILE_EXCLUDE_STAT_FAILED;
  } else {
    /* Check if file is excluded */
    excluded = csync_excluded_traversal(ctx->excludes, path, type);
  }

  if( excluded == CSYNC_NOT_EXCLUDED ) {
      /* Even if it is not excluded by a pattern, maybe it is to be ignored
       * because it's a hidden file that should not be synced.
       * This code should probably be in csync_exclude, but it does not have the fs parameter.
       * Keep it here for now */
      if (ctx->ignore_hidden_files && (fs->flags & CSYNC_VIO_FILE_FLAGS_HIDDEN)) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file excluded because it is a hidden file: %s", path);
          excluded = CSYNC_FILE_EXCLUDE_HIDDEN;
      }
  } else {
      /* File is ignored because it's matched by a user- or system exclude pattern. */
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded  (%d)", path, excluded);
      if (excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
          return 1;
      }
      if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED) {
          return 1;
      }
  }

  if (ctx->current == REMOTE_REPLICA && ctx->callbacks.checkSelectiveSyncBlackListHook) {
      if (ctx->callbacks.checkSelectiveSyncBlackListHook(ctx->callbacks.update_callback_userdata, path)) {
          return 1;
      }
  }

  h = _hash_of_file(ctx, file );
  if( h == 0 ) {
    return -1;
  }
  size = sizeof(csync_file_stat_t) + len + 1;

  st = c_malloc(size);

  /* Set instruction by default to none */
  st->instruction = CSYNC_INSTRUCTION_NONE;
  st->etag = NULL;
  st->child_modified = 0;
  st->has_ignored_files = 0;
  if (type == CSYNC_FTW_TYPE_FILE ) {
    if (fs->mtime == 0) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - mtime is zero!", path);
    }
  }

  if (excluded > CSYNC_NOT_EXCLUDED || type == CSYNC_FTW_TYPE_SLINK) {
      st->instruction = CSYNC_INSTRUCTION_IGNORE;
      if (ctx->current_fs) {
          ctx->current_fs->has_ignored_files = true;
      }

      goto out;
  }

  /* Update detection: Check if a database entry exists.
   * If not, the file is either new or has been renamed. To see if it is
   * renamed, the db gets queried by the inode of the file as that one
   * does not change on rename.
   */
  if (csync_get_statedb_exists(ctx)) {
    tmp = csync_statedb_get_stat_by_hash(ctx, h);

    if(_last_db_return_error(ctx)) {
        csync_file_stat_free(st);
        csync_file_stat_free(tmp);
        ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
        return -1;
    }

    if(tmp && tmp->phash == h ) { /* there is an entry in the database */
        /* we have an update! */
        CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "Database entry found, compare: %" PRId64 " <-> %" PRId64
                                            ", etag: %s <-> %s, inode: %" PRId64 " <-> %" PRId64
                                            ", size: %" PRId64 " <-> %" PRId64 ", perms: %s <-> %s, ignore: %d",
                  ((int64_t) fs->mtime), ((int64_t) tmp->modtime),
                  fs->etag, tmp->etag, (uint64_t) fs->inode, (uint64_t) tmp->inode,
                  (uint64_t) fs->size, (uint64_t) tmp->size, fs->remotePerm, tmp->remotePerm, tmp->has_ignored_files );
        if (ctx->current == REMOTE_REPLICA && !c_streq(fs->etag, tmp->etag)) {
            st->instruction = CSYNC_INSTRUCTION_EVAL;

            // Preserve the EVAL flag later on if the type has changed.
            if (_csync_filetype_different(tmp, fs)) {
                st->child_modified = 1;
            }

            goto out;
        }
        if (ctx->current == LOCAL_REPLICA &&
                (!_csync_mtime_equal(fs->mtime, tmp->modtime)
                 // zero size in statedb can happen during migration
                 || (tmp->size != 0 && fs->size != tmp->size))) {

            // Checksum comparison at this stage is only enabled for .eml files,
            // check #4754 #4755
            bool isEmlFile = csync_fnmatch("*.eml", file, FNM_CASEFOLD) == 0;
            if (isEmlFile && fs->size == tmp->size && tmp->checksumTypeId) {
                if (ctx->callbacks.checksum_hook) {
                    st->checksum = ctx->callbacks.checksum_hook(
                                file, tmp->checksumTypeId,
                                ctx->callbacks.checksum_userdata);
                }
                bool checksumIdentical = false;
                if (st->checksum) {
                    st->checksumTypeId = tmp->checksumTypeId;
                    checksumIdentical = strncmp(st->checksum, tmp->checksum, 1000) == 0;
                }
                if (checksumIdentical) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "NOTE: Checksums are identical, file did not actually change: %s", path);
                    st->instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                    goto out;
                }
            }

            // Preserve the EVAL flag later on if the type has changed.
            if (_csync_filetype_different(tmp, fs)) {
                st->child_modified = 1;
            }

            st->instruction = CSYNC_INSTRUCTION_EVAL;
            goto out;
        }
        bool metadata_differ = (ctx->current == REMOTE_REPLICA && (!c_streq(fs->file_id, tmp->file_id)
                                                            || !c_streq(fs->remotePerm, tmp->remotePerm)))
                             || (ctx->current == LOCAL_REPLICA && fs->inode != tmp->inode);
        if (type == CSYNC_FTW_TYPE_DIR && ctx->current == REMOTE_REPLICA
                && !metadata_differ && ctx->read_remote_from_db) {
            /* If both etag and file id are equal for a directory, read all contents from
             * the database.
             * The metadata comparison ensure that we fetch all the file id or permission when
             * upgrading owncloud
             */
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Reading from database: %s", path);
            ctx->remote.read_from_db = true;
        }
        /* If it was remembered in the db that the remote dir has ignored files, store
         * that so that the reconciler can make advantage of.
         */
        if( ctx->current == REMOTE_REPLICA ) {
            st->has_ignored_files = tmp->has_ignored_files;
        }
        if (metadata_differ) {
            /* file id or permissions has changed. Which means we need to update them in the DB. */
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Need to update metadata for: %s", path);
            st->instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        } else {
            st->instruction = CSYNC_INSTRUCTION_NONE;
        }
    } else {
        enum csync_vio_file_type_e tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;

        /* tmp might point to malloc mem, so free it here before reusing tmp  */
        csync_file_stat_free(tmp);

        /* check if it's a file and has been renamed */
        if (ctx->current == LOCAL_REPLICA) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Checking for rename based on inode # %" PRId64 "", (uint64_t) fs->inode);

            tmp = csync_statedb_get_stat_by_inode(ctx, fs->inode);

            if(_last_db_return_error(ctx)) {
                csync_file_stat_free(st);
                ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
                return -1;
            }

            /* translate the file type between the two stat types csync has. */
            if( tmp && tmp->type == CSYNC_FTW_TYPE_FILE ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_REGULAR;
            } else if( tmp && tmp->type == CSYNC_FTW_TYPE_DIR) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            } else if( tmp && tmp->type == CSYNC_FTW_TYPE_SLINK ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
            } else {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
            }

            // Default to NEW unless we're sure it's a rename.
            st->instruction = CSYNC_INSTRUCTION_NEW;

            bool isRename =
                tmp && tmp->inode == fs->inode && tmp_vio_type == fs->type
                    && (tmp->modtime == fs->mtime || fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY)
#ifdef NO_RENAME_EXTENSION
                    && _csync_sameextension(tmp->path, path)
#endif
                ;


            // Verify the checksum where possible
            if (isRename && tmp->checksumTypeId && ctx->callbacks.checksum_hook
                    && fs->type == CSYNC_VIO_FILE_TYPE_REGULAR) {
                st->checksum = ctx->callbacks.checksum_hook(
                            file, tmp->checksumTypeId,
                            ctx->callbacks.checksum_userdata);
                if (st->checksum) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "checking checksum of potential rename %s %s <-> %s", path, st->checksum, tmp->checksum);
                    st->checksumTypeId = tmp->checksumTypeId;
                    isRename = strncmp(st->checksum, tmp->checksum, 1000) == 0;
                }
            }

            if (isRename) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "pot rename detected based on inode # %" PRId64 "", (uint64_t) fs->inode);
                /* inode found so the file has been renamed */
                st->instruction = CSYNC_INSTRUCTION_EVAL_RENAME;
                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) {
                    csync_rename_record(ctx, tmp->path, path);
                }
            }
            goto out;

        } else {
            /* Remote Replica Rename check */
            tmp = csync_statedb_get_stat_by_file_id(ctx, fs->file_id);

            if(_last_db_return_error(ctx)) {
                csync_file_stat_free(st);
                ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
                return -1;
            }
            if(tmp ) {                           /* tmp existing at all */
                if ( _csync_filetype_different(tmp, fs)) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "file types different is not!");
                    st->instruction = CSYNC_INSTRUCTION_NEW;
                    goto out;
                }
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "remote rename detected based on fileid %s %s", tmp->path, file);
                st->instruction = CSYNC_INSTRUCTION_EVAL_RENAME;
                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) {
                    csync_rename_record(ctx, tmp->path, path);
                } else {
                    if( !c_streq(tmp->etag, fs->etag) ) {
                        /* CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "ETags are different!"); */
                        /* File with different etag, don't do a rename, but download the file again */
                        st->instruction = CSYNC_INSTRUCTION_NEW;
                    }
                }
                goto out;

            } else {
                /* file not found in statedb */
                st->instruction = CSYNC_INSTRUCTION_NEW;

                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY && ctx->current == REMOTE_REPLICA && ctx->callbacks.checkSelectiveSyncNewFolderHook) {
                    if (ctx->callbacks.checkSelectiveSyncNewFolderHook(ctx->callbacks.update_callback_userdata, path, fs->remotePerm)) {
                        csync_file_stat_free(st);
                        return 1;
                    }
                }
                goto out;
            }
        }
    }
  } else  {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Unable to open statedb" );
      csync_file_stat_free(st);
      ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
      return -1;
  }

out:

  /* Set the ignored error string. */
  if (st->instruction == CSYNC_INSTRUCTION_IGNORE) {
      if( type == CSYNC_FTW_TYPE_SLINK ) {
          st->error_status = CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK; /* Symbolic links are ignored. */
      } else {
          if (excluded == CSYNC_FILE_EXCLUDE_LIST) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST; /* File listed on ignore list. */
          } else if (excluded == CSYNC_FILE_EXCLUDE_INVALID_CHAR) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS;  /* File contains invalid characters. */
          } else if (excluded == CSYNC_FILE_EXCLUDE_TRAILING_SPACE) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_TRAILING_SPACE; /* File ends with a trailing space. */
          } else if (excluded == CSYNC_FILE_EXCLUDE_LONG_FILENAME) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME; /* File name is too long. */
          } else if (excluded == CSYNC_FILE_EXCLUDE_HIDDEN ) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_EXCLUDE_HIDDEN;
          } else if (excluded == CSYNC_FILE_EXCLUDE_STAT_FAILED) {
              st->error_status = CSYNC_STATUS_INDIVIDUAL_STAT_FAILED;
          }
      }
  }
  if (st->instruction != CSYNC_INSTRUCTION_NONE
      && st->instruction != CSYNC_INSTRUCTION_IGNORE
      && st->instruction != CSYNC_INSTRUCTION_UPDATE_METADATA
      && type != CSYNC_FTW_TYPE_DIR) {
    st->child_modified = 1;
  }
  ctx->current_fs = st;

  csync_file_stat_free(tmp);
  st->inode = fs->inode;
  st->mode  = fs->mode;
  st->size  = fs->size;
  st->modtime = fs->mtime;
  st->type  = type;
  st->etag   = NULL;
  if( fs->etag ) {
      SAFE_FREE(st->etag);
      st->etag  = c_strdup(fs->etag);
  }
  csync_vio_set_file_id(st->file_id, fs->file_id);
  if (fs->fields & CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADURL) {
      SAFE_FREE(st->directDownloadUrl);
      st->directDownloadUrl = c_strdup(fs->directDownloadUrl);
  }
  if (fs->fields & CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADCOOKIES) {
      SAFE_FREE(st->directDownloadCookies);
      st->directDownloadCookies = c_strdup(fs->directDownloadCookies);
  }
  if (fs->fields & CSYNC_VIO_FILE_STAT_FIELDS_PERM) {
      strncpy(st->remotePerm, fs->remotePerm, REMOTE_PERM_BUF_SIZE);
  }

  st->phash = h;
  st->pathlen = len;
  memcpy(st->path, (len ? path : ""), len + 1);

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (c_rbtree_insert(ctx->local.tree, (void *) st) < 0) {
        csync_file_stat_free(st);
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
        csync_file_stat_free(st);
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
      }
      break;
    default:
      break;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "file: %s, instruction: %s <<=", st->path,
      csync_instruction_str(st->instruction));

  return 0;
}

int csync_walker(CSYNC *ctx, const char *file, const csync_vio_file_stat_t *fs,
    enum csync_ftw_flags_e flag) {
  int rc = -1;
  int type = CSYNC_FTW_TYPE_SKIP;
  csync_file_stat_t *st = NULL;
  uint64_t h;

  if (ctx->abort) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Aborted!");
    ctx->status_code = CSYNC_STATUS_ABORTED;
    return -1;
  }

  switch (flag) {
    case CSYNC_FTW_FLAG_FILE:
      if (ctx->current == REMOTE_REPLICA) {
          if (fs->fields & CSYNC_VIO_FILE_STAT_FIELDS_SIZE) {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s [file_id=%s size=%" PRIu64 "]", file, fs->file_id, fs->size);
          } else {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s [file_id=%s size=UNKNOWN]", file, fs->file_id);
          }
      } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s [inode=%" PRIu64 " size=%" PRIu64 "]", file, fs->inode, fs->size);
      }
      type = CSYNC_FTW_TYPE_FILE;
      break;
  case CSYNC_FTW_FLAG_DIR: /* enter directory */
      if (ctx->current == REMOTE_REPLICA) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "directory: %s [file_id=%s]", file, fs->file_id);
      } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "directory: %s [inode=%" PRIu64 "]", file, fs->inode);
      }
      type = CSYNC_FTW_TYPE_DIR;
      break;
  case CSYNC_FTW_FLAG_NSTAT: /* not statable file */
    /* if file was here before and now is not longer stat-able, still
     * add it to the db, otherwise not. */
    h = _hash_of_file( ctx, file );
    if( h == 0 ) {
      return 0;
    }
    st = csync_statedb_get_stat_by_hash(ctx, h);
    if( !st ) {
      return 0;
    }
    csync_file_stat_free(st);
    st = NULL;

    type = CSYNC_FTW_TYPE_SKIP;
    break;
  case CSYNC_FTW_FLAG_SLINK:
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "symlink: %s - not supported", file);
    type = CSYNC_FTW_TYPE_SLINK;
    break;
  case CSYNC_FTW_FLAG_DNR:
  case CSYNC_FTW_FLAG_DP:
  case CSYNC_FTW_FLAG_SLN:
  default:
    return 0;
    break;
  }

  rc = _csync_detect_update(ctx, file, fs, type );

  return rc;
}

static bool fill_tree_from_db(CSYNC *ctx, const char *uri)
{
    if( csync_statedb_get_below_path(ctx, uri) < 0 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "StateDB could not be read!");
        return false;
    }

    return true;
}

/* set the current item to an ignored state.
 * If the item is set to ignored, the update phase continues, ie. its not a hard error */
static bool mark_current_item_ignored( CSYNC *ctx, csync_file_stat_t *previous_fs, CSYNC_STATUS status )
{
    if(!ctx) {
        return false;
    }

    if (ctx->current_fs) {
        ctx->current_fs->instruction = CSYNC_INSTRUCTION_IGNORE;
        ctx->current_fs->error_status = status;
        /* If a directory has ignored files, put the flag on the parent directory as well */
        if( previous_fs ) {
            previous_fs->has_ignored_files = true;
        }
        return true;
    }
    return false;
}

/* File tree walker */
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn,
    unsigned int depth) {
  char *filename = NULL;
  char *d_name = NULL;
  csync_vio_handle_t *dh = NULL;
  csync_vio_file_stat_t *dirent = NULL;
  csync_file_stat_t *previous_fs = NULL;
  int read_from_db = 0;
  int rc = 0;
  int res = 0;

  if (!depth) {
    mark_current_item_ignored(ctx, previous_fs, CSYNC_STATUS_INDIVIDUAL_TOO_DEEP);
    goto done;
  }

  bool do_read_from_db = (ctx->current == REMOTE_REPLICA && ctx->remote.read_from_db);

  read_from_db = ctx->remote.read_from_db;

  // if the etag of this dir is still the same, its content is restored from the
  // database.
  if( do_read_from_db ) {
      if( ! fill_tree_from_db(ctx, uri) ) {
        errno = ENOENT;
        ctx->status_code = CSYNC_STATUS_OPENDIR_ERROR;
        goto error;
      }
      goto done;
  }

  if ((dh = csync_vio_opendir(ctx, uri)) == NULL) {
      if (ctx->abort) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Aborted!");
          ctx->status_code = CSYNC_STATUS_ABORTED;
          goto error;
      }
      int asp = 0;
      /* permission denied */
      ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_OPENDIR_ERROR);
      if (errno == EACCES) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Permission denied.");
          if (mark_current_item_ignored(ctx, previous_fs, CSYNC_STATUS_PERMISSION_DENIED)) {
              goto done;
          }
      } else if(errno == ENOENT) {
          asp = asprintf( &ctx->error_string, "%s", uri);
          if (asp < 0) {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "asprintf failed!");
          }
      }
      // 403 Forbidden can be sent by the server if the file firewall is active.
      // A file or directory should be ignored and sync must continue. See #3490
      else if(errno == ERRNO_FORBIDDEN) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Directory access Forbidden (File Firewall?)");
          if( mark_current_item_ignored(ctx, previous_fs, CSYNC_STATUS_FORBIDDEN) ) {
              goto done;
          }
          /* if current_fs is not defined here, better throw an error */
      }
      // The server usually replies with the custom "503 Storage not available"
      // if some path is temporarily unavailable. But in some cases a standard 503
      // is returned too. Thus we can't distinguish the two and will treat any
      // 503 as request to ignore the folder. See #3113 #2884.
      else if(errno == ERRNO_STORAGE_UNAVAILABLE || errno == ERRNO_SERVICE_UNAVAILABLE) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Storage was not available!");
          if( mark_current_item_ignored(ctx, previous_fs, CSYNC_STATUS_STORAGE_UNAVAILABLE ) ) {
              goto done;
          }
          /* if current_fs is not defined here, better throw an error */
      } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "opendir failed for %s - errno %d", uri, errno);
      }
      goto error;
  }

  while ((dirent = csync_vio_readdir(ctx, dh))) {
    int flen;
    int flag;

    /* Conversion error */
    if (dirent->name == NULL && dirent->original_name) {
        ctx->status_code = CSYNC_STATUS_INVALID_CHARACTERS;
        ctx->error_string = dirent->original_name; // take ownership
        dirent->original_name = NULL;
        goto error;
    }

    d_name = dirent->name;
    if (d_name == NULL) {
      ctx->status_code = CSYNC_STATUS_READDIR_ERROR;
      goto error;
    }

    /* skip "." and ".." */
    if ( (d_name[0] == '.' && d_name[1] == '\0')
          || (d_name[0] == '.' && d_name[1] == '.' && d_name[2] == '\0')) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      continue;
    }

    if (uri[0] == '\0') {
      filename = c_strdup(d_name);
      flen = strlen(d_name);
    } else {
      flen = asprintf(&filename, "%s/%s", uri, d_name);
    }
    if (flen < 0 || !filename) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
      goto error;
    }

    /* Only for the local replica we have to stat(), for the remote one we have all data already */
    if (ctx->replica == LOCAL_REPLICA) {
        res = csync_vio_stat(ctx, filename, dirent);
    } else {
        res = 0;
    }

    /* if the filename starts with a . we consider it a hidden file
     * For windows, the hidden state is also discovered within the vio
     * local stat function.
     */
    if( d_name[0] == '.' ) {
        if (strcmp(".sys.admin#recall#", d_name) != 0) { /* recall file shall not be ignored (#4420) */
            dirent->flags |= CSYNC_VIO_FILE_FLAGS_HIDDEN;
        }
    }

    if( res == 0) {
      switch (dirent->type) {
        case CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK:
          flag = CSYNC_FTW_FLAG_SLINK;
          break;
        case CSYNC_VIO_FILE_TYPE_DIRECTORY:
          flag = CSYNC_FTW_FLAG_DIR;
          break;
        case CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE:
        case CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE:
        case CSYNC_VIO_FILE_TYPE_SOCKET:
          flag = CSYNC_FTW_FLAG_SPEC;
          break;
        case CSYNC_VIO_FILE_TYPE_FIFO:
          flag = CSYNC_FTW_FLAG_SPEC;
          break;
        default:
          flag = CSYNC_FTW_FLAG_FILE;
          break;
      };
    } else {
      flag = CSYNC_FTW_FLAG_NSTAT;
    }

    previous_fs = ctx->current_fs;

    /* Call walker function for each file */
    rc = fn(ctx, filename, dirent, flag);
    /* this function may update ctx->current and ctx->read_from_db */

    if (rc < 0) {
      if (CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = CSYNC_STATUS_UPDATE_ERROR;
      }

      ctx->current_fs = previous_fs;
      goto error;
    }

    if (flag == CSYNC_FTW_FLAG_DIR && rc == 0
        && (!ctx->current_fs || ctx->current_fs->instruction != CSYNC_INSTRUCTION_IGNORE)) {
      rc = csync_ftw(ctx, filename, fn, depth - 1);
      if (rc < 0) {
        ctx->current_fs = previous_fs;
        goto error;
      }

      if (ctx->current_fs && !ctx->current_fs->child_modified
          && ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL) {
          if (ctx->current == REMOTE_REPLICA) {
              ctx->current_fs->instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
          } else {
              ctx->current_fs->instruction = CSYNC_INSTRUCTION_NONE;
          }
      }

      if (ctx->current_fs && previous_fs && ctx->current_fs->has_ignored_files) {
          /* If a directory has ignored files, put the flag on the parent directory as well */
          previous_fs->has_ignored_files = ctx->current_fs->has_ignored_files;
      }
    }

    if (ctx->current_fs && previous_fs && ctx->current_fs->child_modified) {
        /* If a directory has modified files, put the flag on the parent directory as well */
        previous_fs->child_modified = ctx->current_fs->child_modified;
    }

    ctx->current_fs = previous_fs;
    ctx->remote.read_from_db = read_from_db;
    SAFE_FREE(filename);
    csync_vio_file_stat_destroy(dirent);
    dirent = NULL;
  }

  csync_vio_closedir(ctx, dh);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, " <= Closing walk for %s with read_from_db %d", uri, read_from_db);

done:
  csync_vio_file_stat_destroy(dirent);
  SAFE_FREE(filename);
  return rc;
error:
  ctx->remote.read_from_db = read_from_db;
  if (dh != NULL) {
    csync_vio_closedir(ctx, dh);
  }
  SAFE_FREE(filename);
  return -1;
}

/* vim: set ts=8 sw=2 et cindent: */
