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

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

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

  /* Check if file is excluded */
  excluded = csync_excluded(ctx, path,type);

  if (excluded != CSYNC_NOT_EXCLUDED) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded  (%d)", path, excluded);
    if (excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
      switch (ctx->current) {
        case LOCAL_REPLICA:
          ctx->local.ignored_cleanup = c_list_append(ctx->local.ignored_cleanup, c_strdup(path));
          break;
        case REMOTE_REPLICA:
          ctx->remote.ignored_cleanup = c_list_append(ctx->remote.ignored_cleanup, c_strdup(path));
          break;
        default:
          break;
      }
      return 0;
    }
    if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED) {
        return 0;
    }
  }

  h = _hash_of_file(ctx, file );
  if( h == 0 ) {
    return -1;
  }
  size = sizeof(csync_file_stat_t) + len + 1;

  st = c_malloc(size);
  if (st == NULL) {
    ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
    return -1;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "==> file: %s - hash %llu, mtime: %llu, fileId: %s",
      path, (unsigned long long ) h, (unsigned long long) fs->mtime, fs->file_id);

  /* Set instruction by default to none */
  st->instruction = CSYNC_INSTRUCTION_NONE;
  st->etag = NULL;
  st->child_modified = 0;

  /* check hardlink count */
  if (type == CSYNC_FTW_TYPE_FILE ) {
    if( fs->nlink > 1) {
      st->instruction = CSYNC_INSTRUCTION_IGNORE;
      goto out;
    }

    if (fs->mtime == 0) {
      tmp = csync_statedb_get_stat_by_hash(ctx->statedb.db, h);
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s - mtime is zero!", path);
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
    tmp = csync_statedb_get_stat_by_hash(ctx->statedb.db, h);

    if(tmp && tmp->phash == h ) { /* there is an entry in the database */
        /* we have an update! */
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Database entry found, compare: %" PRId64 " <-> %" PRId64 ", etag: %s <-> %s, inode: %" PRId64 " <-> %" PRId64,
                  ((int64_t) fs->mtime), ((int64_t) tmp->modtime), fs->etag, tmp->etag, (uint64_t) fs->inode, (uint64_t) tmp->inode);
        if( !fs->etag) {
            st->instruction = CSYNC_INSTRUCTION_EVAL;
            goto out;
        }
        if((ctx->current == REMOTE_REPLICA && !c_streq(fs->etag, tmp->etag ))
            || (ctx->current == LOCAL_REPLICA && (fs->mtime != tmp->modtime
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
        if (type == CSYNC_FTW_TYPE_DIR && ctx->current == REMOTE_REPLICA
                && c_streq(fs->file_id, tmp->file_id)) {
            /* If both etag and file id are equal for a directory, read all contents from
             * the database. */
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Reading from database: %s", path);
            ctx->remote.read_from_db = true;
        }
        st->instruction = CSYNC_INSTRUCTION_NONE;
    } else {
        enum csync_vio_file_type_e tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;

        /* check if it's a file and has been renamed */
        if (ctx->current == LOCAL_REPLICA) {
            tmp = csync_statedb_get_stat_by_inode(ctx->statedb.db, fs->inode);

            /* translate the file type between the two stat types csync has. */
            if( tmp && tmp->type == 0 ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_REGULAR;
            } else if( tmp && tmp->type == 2 ) {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            } else {
                tmp_vio_type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
            }

            if (tmp && tmp->inode == fs->inode && tmp_vio_type == fs->type
                    && (tmp->modtime == fs->mtime || fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY)) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "inodes: %" PRId64 " <-> %" PRId64, (uint64_t) tmp->inode, (uint64_t) fs->inode);
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
            tmp = csync_statedb_get_stat_by_file_id(ctx->statedb.db, fs->file_id);
            if(tmp ) {                           /* tmp existing at all */
                if ((tmp->type == CSYNC_FTW_TYPE_DIR && fs->type != CSYNC_VIO_FILE_TYPE_DIRECTORY) ||
                        (tmp->type == CSYNC_FTW_TYPE_FILE && fs->type != CSYNC_VIO_FILE_TYPE_REGULAR)) {
                    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "WARN: file types different is not!");
                    st->instruction = CSYNC_INSTRUCTION_NEW;
                    goto out;
                }
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
                goto out;
            }
        }
    }
  } else  {
      st->instruction = CSYNC_INSTRUCTION_NEW;
  }

out:

  /* Set the ignored error string. */
  if (st->instruction == CSYNC_INSTRUCTION_IGNORE) {
    if (excluded == CSYNC_FILE_EXCLUDE_LIST) {
      st->error_status = CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST; /* File listed on ignore list. */
    } else if (excluded == CSYNC_FILE_EXCLUDE_INVALID_CHAR) {
      st->error_status = CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS;  /* File contains invalid characters. */
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
  st->uid   = fs->uid;
  st->gid   = fs->gid;
  st->nlink = fs->nlink;
  st->type  = type;
  st->etag   = NULL;
  if( fs->etag ) {
      st->etag  = c_strdup(fs->etag);
  }
  csync_vio_set_file_id(st->file_id, fs->file_id);

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
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "file: %s", file);
      type = CSYNC_FTW_TYPE_FILE;
      break;
  case CSYNC_FTW_FLAG_DIR: /* enter directory */
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "directory: %s", file);
      type = CSYNC_FTW_TYPE_DIR;
      break;
  case CSYNC_FTW_FLAG_NSTAT: /* not statable file */
    /* if file was here before and now is not longer stat-able, still
     * add it to the db, otherwise not. */
    h = _hash_of_file( ctx, file );
    if( h == 0 ) {
      return 0;
    }
    st = csync_statedb_get_stat_by_hash(ctx->statedb.db, h);
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

/* File tree walker */
int csync_ftw(CSYNC *ctx, const char *uri, csync_walker_fn fn,
    unsigned int depth) {
  char errbuf[256] = {0};
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

  if ((dh = csync_vio_opendir(ctx, uri)) == NULL) {
    /* permission denied */
    ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_OPENDIR_ERROR);
    if (errno == EACCES) {
       return 0;
    } else if(errno == EIO ) {
      /* Proxy problems (ownCloud) */
      ctx->status_code = CSYNC_STATUS_PROXY_ERROR;
    } else if(errno == ENOENT) {
        asprintf( &ctx->error_string, "%s", uri);
    } else {
      C_STRERROR(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "opendir failed for %s - %s (errno %d)",
          uri, errbuf, errno);
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
    if (c_streq(path, ".csync_journal.db")
            || c_streq(path, ".csync_journal.db.ctmp")
            || c_streq(path, ".csync_journal.db.ctmp-journal")
            || c_streq(path, ".csync-progressdatabase")) {
        csync_vio_file_stat_destroy(dirent);
        dirent = NULL;
        SAFE_FREE(filename);
        continue;
    }

    /* == see if really stat has to be called. */
    if( do_read_from_db ) {
        fs = dirent;
        res = 0;
    } else {
        fs = csync_vio_file_stat_new();
        res = csync_vio_stat(ctx, filename, fs);
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
        etag = csync_statedb_get_uniqId( ctx, h, fs );
        if( etag ) {
            SAFE_FREE(fs->etag);
            fs->etag = etag;
            fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;
        }
        if( c_streq(etag, "")) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database is EMPTY: %s", path);
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database: %s -> %s", path, fs->etag ? fs->etag : "<NULL>" );
        }
    }

    previous_fs = ctx->current_fs;

    /* Call walker function for each file */
    rc = fn(ctx, filename, fs, flag);
    /* this function may update ctx->current and ctx->read_from_db */

    if (ctx->current_fs && previous_fs && ctx->current_fs->child_modified) {
        previous_fs->child_modified = ctx->current_fs->child_modified;
    }

    if( ! do_read_from_db ) {
        csync_vio_file_stat_destroy(fs);
    } else {
        SAFE_FREE(fs->etag);
    }

    if (rc < 0) {
      if (CSYNC_STATUS_IS_OK(ctx->status_code)) {
          ctx->status_code = CSYNC_STATUS_UPDATE_ERROR;
      }

      csync_vio_closedir(ctx, dh);
      ctx->current_fs = previous_fs;
      goto done;
    }

    if (flag == CSYNC_FTW_FLAG_DIR && depth
        && (!ctx->current_fs || ctx->current_fs->instruction != CSYNC_INSTRUCTION_IGNORE)) {
      rc = csync_ftw(ctx, filename, fn, depth - 1);
      if (rc < 0) {
        ctx->current_fs = previous_fs;
        csync_vio_closedir(ctx, dh);
        goto done;
      }

      if (ctx->current_fs && !ctx->current_fs->child_modified
          && ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL) {
        ctx->current_fs->instruction = CSYNC_INSTRUCTION_NONE;
        ctx->current_fs->should_update_etag = true;
      }
    }

    if (ctx->current_fs && (ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL ||
        ctx->current_fs->instruction == CSYNC_INSTRUCTION_NEW)) {
        ctx->current_fs->should_update_etag = true;
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
  ctx->remote.read_from_db = read_from_db;
  csync_vio_file_stat_destroy(dirent);
  SAFE_FREE(filename);
  return rc;
error:
  if (dh != NULL) {
    csync_vio_closedir(ctx, dh);
  }
  ctx->remote.read_from_db = read_from_db;
  SAFE_FREE(filename);
  return -1;
}

/* vim: set ts=8 sw=2 et cindent: */
