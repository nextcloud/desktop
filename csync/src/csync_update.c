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
    switch (ctx->current) {
    case LOCAL_REPLICA:
      if (strlen(path) <= strlen(ctx->local.uri)) {
        return 0;
      }
      path += strlen(ctx->local.uri) + 1;
      break;
    case REMOTE_REPLICA:
      if (strlen(path) <= strlen(ctx->remote.uri)) {
        return 0;
      }
      path += strlen(ctx->remote.uri) + 1;
      break;
    default:
      path = NULL;
      return 0;
      break;
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
  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (strlen(path) <= strlen(ctx->local.uri)) {
        ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
        return -1;
      }
      path += strlen(ctx->local.uri) + 1;
      break;
    case REMOTE_REPLICA:
      if (strlen(path) <= strlen(ctx->remote.uri)) {
        ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
        return -1;
      }
      path += strlen(ctx->remote.uri) + 1;
      break;
    default:
      path = NULL;
      ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
      return -1;
  }

  len = strlen(path);

  /* This code should probably be in csync_exclude, but it does not have the fs parameter.
     Keep it here for now and TODO also find out if we want this for Windows
     https://github.com/owncloud/mirall/issues/2086 */
  if (fs->flags & CSYNC_VIO_FILE_FLAGS_HIDDEN) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file excluded because it is a hidden file: %s", path);
      return 0;
  }

  /* Check if file is excluded */
  excluded = csync_excluded(ctx, path,type);

  if (excluded != CSYNC_NOT_EXCLUDED) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded  (%d)", path, excluded);
    if (excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
        return 1;
    }
    if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED) {
        return 1;
    }

    if (ctx->current_fs) {
        ctx->current_fs->has_ignored_files = true;
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

  /* check hardlink count */
  if (type == CSYNC_FTW_TYPE_FILE ) {

    if (fs->mtime == 0) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - mtime is zero!", path);

      tmp = csync_statedb_get_stat_by_hash(ctx, h);
      if(_last_db_return_error(ctx)) {
          SAFE_FREE(st);
          SAFE_FREE(tmp);
          ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
          return -1;
      }

      if (tmp == NULL) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - not found in db, IGNORE!", path);
        st->instruction = CSYNC_INSTRUCTION_IGNORE;
      } else {
        SAFE_FREE(st);
        st = tmp;
        st->instruction = CSYNC_INSTRUCTION_NONE;
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - tmp non zero, mtime %lu", path, st->modtime );
        tmp = NULL;
      }
      goto fastout; /* Skip copying of the etag. That's an important difference to upstream
                     * without etags. */
    }
  }

  /* Ignore non statable files and other strange cases. */
  if (type == CSYNC_FTW_TYPE_SKIP) {
    st->instruction = CSYNC_INSTRUCTION_NONE;
    goto out;
  }
  if (excluded > CSYNC_NOT_EXCLUDED || type == CSYNC_FTW_TYPE_SLINK) {
      if( type == CSYNC_FTW_TYPE_SLINK ) {
          st->error_status = CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK; /* Symbolic links are ignored. */
      }
      st->instruction = CSYNC_INSTRUCTION_IGNORE;
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
        SAFE_FREE(st);
        ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
        return -1;
    }

    if(tmp && tmp->phash == h ) { /* there is an entry in the database */
        /* we have an update! */
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Database entry found, compare: %" PRId64 " <-> %" PRId64
                                            ", etag: %s <-> %s, inode: %" PRId64 " <-> %" PRId64
                                            ", size: %" PRId64 " <-> %" PRId64 ", perms: %s <-> %s",
                  ((int64_t) fs->mtime), ((int64_t) tmp->modtime),
                  fs->etag, tmp->etag, (uint64_t) fs->inode, (uint64_t) tmp->inode,
                  (uint64_t) fs->size, (uint64_t) tmp->size, fs->remotePerm, tmp->remotePerm );
        if( !fs->etag) {
            st->instruction = CSYNC_INSTRUCTION_EVAL;
            goto out;
        }
        if((ctx->current == REMOTE_REPLICA && !c_streq(fs->etag, tmp->etag ))
            || (ctx->current == LOCAL_REPLICA && (!_csync_mtime_equal(fs->mtime, tmp->modtime)
                                                  // zero size in statedb can happen during migration
                                                  || (tmp->size != 0 && fs->size != tmp->size)
#if 0
                                                  || fs->inode != tmp->inode
#endif
                                                  ))) {
            /* Comparison of the local inode is disabled because people reported problems
             * on windows with flacky inode values, see github bug #779
             *
             * The inode needs to be observed because:
             * $>  echo a > a.txt ; echo b > b.txt
             * both files have the same mtime
             * sync them.
             * $> rm a.txt && mv b.txt a.txt
             * makes b.txt appearing as a.txt yet a sync is not performed because
             * both have the same modtime as mv does not change that.
             */
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
        if (metadata_differ) {
            /* file id or permissions has changed. Which means we need to update them in the DB. */
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Need to update metadata for: %s", path);
            st->should_update_metadata = true;
        }
        st->instruction = CSYNC_INSTRUCTION_NONE;
    } else {
        enum csync_vio_file_type_e tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;

        /* tmp might point to malloc mem, so free it here before reusing tmp  */
        SAFE_FREE(tmp);

        /* check if it's a file and has been renamed */
        if (ctx->current == LOCAL_REPLICA) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Checking for rename based on inode # %" PRId64 "", (uint64_t) fs->inode);

            tmp = csync_statedb_get_stat_by_inode(ctx, fs->inode);

            if(_last_db_return_error(ctx)) {
                SAFE_FREE(st);
                ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
                return -1;
            }

            /* translate the file type between the two stat types csync has. */
            if( tmp && tmp->type == 0 ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_REGULAR;
            } else if( tmp && tmp->type == 2 ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            } else {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
            }

            if (tmp && tmp->inode == fs->inode && tmp_vio_type == fs->type
                    && (tmp->modtime == fs->mtime || fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY)
#ifdef NO_RENAME_EXTENSION
                    && _csync_sameextension(tmp->path, path)
#endif
               ) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "pot rename detected based on inode # %" PRId64 "", (uint64_t) fs->inode);
                /* inode found so the file has been renamed */
                st->instruction = CSYNC_INSTRUCTION_EVAL_RENAME;
                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) {
                    csync_rename_record(ctx, tmp->path, path);
                }
                goto out;
            } else {
                /* file not found in statedb */
                st->instruction = CSYNC_INSTRUCTION_NEW;
                goto out;
            }
        } else {
            /* Remote Replica Rename check */
            tmp = csync_statedb_get_stat_by_file_id(ctx, fs->file_id);

            if(_last_db_return_error(ctx)) {
                SAFE_FREE(st);
                ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
                return -1;
            }
            if(tmp ) {                           /* tmp existing at all */
                if ((tmp->type == CSYNC_FTW_TYPE_DIR && fs->type != CSYNC_VIO_FILE_TYPE_DIRECTORY) ||
                        (tmp->type == CSYNC_FTW_TYPE_FILE && fs->type != CSYNC_VIO_FILE_TYPE_REGULAR)) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "WARN: file types different is not!");
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

                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY && ctx->current == REMOTE_REPLICA && ctx->callbacks.checkSelectiveSyncNewShareHook) {
                    if (strchr(fs->remotePerm, 'S') != NULL) { /* check that the directory is shared */
                        if (ctx->callbacks.checkSelectiveSyncNewShareHook(ctx->callbacks.update_callback_userdata, path)) {
                            SAFE_FREE(st);
                            return 1;
                        }
                    }
                }
                goto out;
            }
        }
    }
  } else  {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Unable to open statedb" );
      SAFE_FREE(st);
      ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
      return -1;
  }

out:

  /* Set the ignored error string. */
  if (st->instruction == CSYNC_INSTRUCTION_IGNORE) {
    if (excluded == CSYNC_FILE_EXCLUDE_LIST) {
      st->error_status = CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST; /* File listed on ignore list. */
    } else if (excluded == CSYNC_FILE_EXCLUDE_INVALID_CHAR) {
      st->error_status = CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS;  /* File contains invalid characters. */
    } else if (excluded == CSYNC_FILE_EXCLUDE_LONG_FILENAME) {
      st->error_status = CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME; /* File name is too long. */
    }
  }
  if (st->instruction != CSYNC_INSTRUCTION_NONE && st->instruction != CSYNC_INSTRUCTION_IGNORE
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

fastout:  /* target if the file information is read from database into st */
  st->phash = h;
  st->pathlen = len;
  memcpy(st->path, (len ? path : ""), len + 1);

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (c_rbtree_insert(ctx->local.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
      }
      break;
    default:
      break;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: %s <<=", st->path,
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s [file_id=%s size=%" PRIu64 "]", file, fs->file_id, fs->size);
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
    const char *path = NULL;

    if( strlen(uri) < strlen(ctx->remote.uri)+1) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "name does not contain remote uri!");
        return false;
    }

    path = uri + strlen(ctx->remote.uri)+1;

    if( csync_statedb_get_below_path(ctx, path) < 0 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "StateDB could not be read!");
        return false;
    }

    return true;
}

/* File tree walker */
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn,
    unsigned int depth) {
  char *filename = NULL;
  char *d_name = NULL;
  csync_vio_handle_t *dh = NULL;
  csync_vio_file_stat_t *dirent = NULL;
  csync_vio_file_stat_t *fs = NULL;
  csync_file_stat_t *previous_fs = NULL;
  int read_from_db = 0;
  int rc = 0;
  int res = 0;

  bool do_read_from_db = (ctx->current == REMOTE_REPLICA && ctx->remote.read_from_db);

  if (uri[0] == '\0') {
    errno = ENOENT;
    ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
    goto error;
  }

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

  const char *uri_for_vio = uri;
  if (ctx->current == REMOTE_REPLICA) {
      uri_for_vio += strlen(ctx->remote.uri);
      if (strlen(uri_for_vio) > 0 && uri_for_vio[0] == '/') {
          uri_for_vio++; // cut leading slash
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "URI without fuzz for %s is \"%s\"", uri, uri_for_vio);
  }

  if ((dh = csync_vio_opendir(ctx, uri_for_vio)) == NULL) {
      if (ctx->abort) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Aborted!");
          ctx->status_code = CSYNC_STATUS_ABORTED;
          goto error;
      }
      int asp = 0;
      /* permission denied */
      ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_OPENDIR_ERROR);
      if (errno == EACCES) {
          return 0;
      } else if(errno == ENOENT) {
          asp = asprintf( &ctx->error_string, "%s", uri);
          if (asp < 0) {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "asprintf failed!");
          }
      }
      // The server usually replies with the custom "503 Storage not available"
      // if some path is temporarily unavailable. But in some cases a standard 503
      // is returned too. Thus we can't distinguish the two and will treat any
      // 503 as request to ignore the folder. See #3113 #2884.
      else if(errno == ERRNO_STORAGE_UNAVAILABLE || errno == ERRNO_SERVICE_UNAVAILABLE) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Storage was not available!");
          if (ctx->current_fs) {
              ctx->current_fs->instruction = CSYNC_INSTRUCTION_IGNORE;
              ctx->current_fs->error_status = CSYNC_STATUS_STORAGE_UNAVAILABLE;
              /* If a directory has ignored files, put the flag on the parent directory as well */
              if( previous_fs ) {
                  previous_fs->has_ignored_files = true;
              }
              goto done;
          }
      } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "opendir failed for %s - errno %d", uri, errno);
      }
      goto error;
  }

  while ((dirent = csync_vio_readdir(ctx, dh))) {
    const char *path = NULL;
    size_t ulen = 0;
    int flen;
    int flag;

    d_name = dirent->name;
    if (d_name == NULL) {
      ctx->status_code = CSYNC_STATUS_READDIR_ERROR;
      goto error;
    }

    /* skip "." and ".." */
    if (d_name[0] == '.' && (d_name[1] == '\0'
          || (d_name[1] == '.' && d_name[2] == '\0'))) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      continue;
    }

    flen = asprintf(&filename, "%s/%s", uri, d_name);
    if (flen < 0) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
      goto error;
    }

    /* Create relative path */
    switch (ctx->current) {
      case LOCAL_REPLICA:
        ulen = strlen(ctx->local.uri) + 1;
        break;
      case REMOTE_REPLICA:
        ulen = strlen(ctx->remote.uri) + 1;
        break;
      default:
        break;
    }

    if (((size_t)flen) < ulen) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
      goto error;
    }

    path = filename + ulen;

    /* skip ".csync_journal.db" and ".csync_journal.db.ctmp" */
    /* Isn't this done via csync_exclude already? */
    if (c_streq(path, ".csync_journal.db")
            || c_streq(path, ".csync_journal.db.ctmp")
            || c_streq(path, ".csync_journal.db.ctmp-journal")
            || c_streq(path, ".csync-progressdatabase")
            || c_streq(path, ".csync_journal.db-shm")
            || c_streq(path, ".csync_journal.db-wal")
            || c_streq(path, ".csync_journal.db-journal")) {
        csync_vio_file_stat_destroy(dirent);
        dirent = NULL;
        SAFE_FREE(filename);
        continue;
    }

    /* Only for the local replica we have to stat(), for the remote one we have all data already */
    if (ctx->replica == LOCAL_REPLICA) {
        fs = csync_vio_file_stat_new();
        res = csync_vio_stat(ctx, filename, fs);
    } else {
        fs = dirent;
        res = 0;
    }

    if( res == 0) {
      switch (fs->type) {
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

    if( ctx->current == LOCAL_REPLICA ) {
        char *etag = NULL;
        int len = strlen( path );
        uint64_t h = c_jhash64((uint8_t *) path, len, 0);
        etag = csync_statedb_get_etag( ctx, h );

        if(_last_db_return_error(ctx)) {
            ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
            SAFE_FREE(etag);
            goto error;
        }

        if( etag ) {
            SAFE_FREE(fs->etag);
            fs->etag = etag;
            fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;

            if( c_streq(etag, "")) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database is EMPTY: %s", path);
            } else {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database: %s -> %s", path, fs->etag ? fs->etag : "<NULL>" );
            }
        }
    }

    previous_fs = ctx->current_fs;

    /* Call walker function for each file */
    rc = fn(ctx, filename, fs, flag);
    /* this function may update ctx->current and ctx->read_from_db */

    /* Only for the local replica we have to destroy stat(), for the remote one it is a pointer to dirent */
    if (ctx->replica == LOCAL_REPLICA) {
        csync_vio_file_stat_destroy(fs);
    }

    if (rc < 0) {
      if (CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = CSYNC_STATUS_UPDATE_ERROR;
      }

      ctx->current_fs = previous_fs;
      goto error;
    }

    if (flag == CSYNC_FTW_FLAG_DIR && depth && rc == 0
        && (!ctx->current_fs || ctx->current_fs->instruction != CSYNC_INSTRUCTION_IGNORE)) {
      rc = csync_ftw(ctx, filename, fn, depth - 1);
      if (rc < 0) {
        ctx->current_fs = previous_fs;
        goto error;
      }

      if (ctx->current_fs && !ctx->current_fs->child_modified
          && ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL) {
        ctx->current_fs->instruction = CSYNC_INSTRUCTION_NONE;
        if (ctx->current == REMOTE_REPLICA) {
          ctx->current_fs->should_update_metadata = true;
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

    if (flag == CSYNC_FTW_FLAG_DIR && ctx->current_fs
        && (ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL ||
            ctx->current_fs->instruction == CSYNC_INSTRUCTION_NEW)) {
        ctx->current_fs->should_update_metadata = true;
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
