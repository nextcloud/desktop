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
  int excluded;

  if ((file == NULL) || (fs == NULL)) {
    errno = EINVAL;
    return -1;
  }

  path = file;
  switch (ctx->current) {
  case LOCAL_REPLICA:
    if (strlen(path) <= strlen(ctx->local.uri)) {
      return -1;
    }
    path += strlen(ctx->local.uri) + 1;
    break;
  case REMOTE_REPLICA:
    if (strlen(path) <= strlen(ctx->remote.uri)) {
      return -1;
    }
    path += strlen(ctx->remote.uri) + 1;
    break;
  default:
    path = NULL;
    return -1;
    break;
  }

  len = strlen(path);

  /* Check if file is excluded */
  excluded = csync_excluded(ctx, path);
  if (excluded) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded  (%d)", path, excluded);
    if (excluded == 2) {
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
    }
  }

  h = _hash_of_file(ctx, file );
  if( h == 0 ) {
    return -1;
  }
  size = sizeof(csync_file_stat_t) + len + 1;

  st = c_malloc(size);
  if (st == NULL) {
    return -1;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "==> file: %s - hash %llu, mtime: %llu",
      path, (unsigned long long ) h, (unsigned long long) fs->mtime);

  /* Set instruction by default to none */
  st->instruction = CSYNC_INSTRUCTION_NONE;
  st->md5 = NULL;
  st->child_modified = 0;

  /* check hardlink count */
  if (type == CSYNC_FTW_TYPE_FILE ) {
    if( fs->nlink > 1) {
      st->instruction = CSYNC_INSTRUCTION_IGNORE;
      goto out;
    }

    if (fs->mtime == 0) {
      tmp = csync_statedb_get_stat_by_hash(ctx, h);
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
  if (excluded > 0 || type == CSYNC_FTW_TYPE_SLINK) {
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
#if 0
    /* this code could possibly replace the one in csync_vio.c stat and would be more efficient */
    if(tmp) {
        if( ctx->current == LOCAL_REPLICA ) {
            if(fs->mtime == tmp->modtime && fs->size == tmp->size) {
                /* filesystem modtime is still the same as the db mtime
                 * thus the md5 sum is still valid. */
                fs->md5 = c_strdup( tmp->md5 );
            }
        }
    }
#endif
    if(tmp && tmp->phash == h ) { /* there is an entry in the database */
        /* we have an update! */
        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Database entry found, compare: %" PRId64 " <-> %" PRId64 ", md5: %s <-> %s, inode: %" PRId64 " <-> %" PRId64,
                  ((int64_t) fs->mtime), ((int64_t) tmp->modtime), fs->md5, tmp->md5, (int64_t) fs->inode, (int64_t) tmp->inode);
        if( !fs->md5) {
            st->instruction = CSYNC_INSTRUCTION_EVAL;
            goto out;
        }
        if((ctx->current == REMOTE_REPLICA && !c_streq(fs->md5, tmp->md5 ))
            || (ctx->current == LOCAL_REPLICA && (fs->mtime != tmp->modtime || fs->inode != tmp->inode))) {
            // if (!fs->mtime > tmp->modtime) {
            st->instruction = CSYNC_INSTRUCTION_EVAL;
            goto out;
        }
        st->instruction = CSYNC_INSTRUCTION_NONE;
    } else {
        /* check if it's a file and has been renamed */
        if (ctx->current == LOCAL_REPLICA) {
            tmp = csync_statedb_get_stat_by_inode(ctx, fs->inode);
            if (tmp && tmp->inode == fs->inode && (tmp->modtime == fs->mtime || fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY)) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "inodes: %" PRId64 " <-> %" PRId64, (int64_t) tmp->inode, (int64_t) fs->inode);
                /* inode found so the file has been renamed */
                st->instruction = CSYNC_INSTRUCTION_RENAME;
                if (fs->type == CSYNC_VIO_FILE_TYPE_DIRECTORY) {
                    csync_rename_record(ctx, tmp->path, path);
                }
                goto out;
            } else {
                /* file not found in statedb */
                st->instruction = CSYNC_INSTRUCTION_NEW;
                goto out;
            }
        }
        /* directory, remote and file not found in statedb */
        st->instruction = CSYNC_INSTRUCTION_NEW;
    }
  } else  {
      st->instruction = CSYNC_INSTRUCTION_NEW;
  }

out:

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
  st->md5   = NULL;
  if( fs->md5 ) {
      st->md5  = c_strdup(fs->md5);
  }

fastout:  /* target if the file information is read from database into st */
  st->phash = h;
  st->pathlen = len;
  memcpy(st->path, (len ? path : ""), len + 1);

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (c_rbtree_insert(ctx->local.tree, (void *) st) < 0) {
        SAFE_FREE(st);
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
        SAFE_FREE(st);
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
    ctx->error_code = CSYNC_ERR_ABORTED;
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

/* check if the dirent entries for the directory can be read from db
 * instead really calling readdir which is costly over net.
 * For that, a single HEAD request is done on the directory to get its
 * id. If the ID has not changed remotely, this subtree hasn't changed
 * and can be read from db.
 */
static int _check_read_from_db(CSYNC *ctx, const char *uri) {
    int len;
    uint64_t h;
    csync_vio_file_stat_t *fs = NULL;
    const char *md5_local  = NULL;
    const char *md5_remote = NULL;
    const char *mpath;
    int rc = 0; /* FIXME: Error handling! */
    csync_file_stat_t* tmp = NULL;

    if( !c_streq( ctx->remote.uri, uri )) {
        /* FIXME: The top uri can not be checked because there is no db entry for it */
        if( strlen(uri) < strlen(ctx->remote.uri)+1 ) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "check_read_from_db: uri is not a remote uri.");
            /* FIXME: errno? */
            return -1;
        }
        mpath = uri + strlen(ctx->remote.uri) + 1;
        fs = csync_vio_file_stat_new();
        if(fs == NULL) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "check_read_from_db: memory fault.");
            errno = ENOMEM;
            return -1;
        }
        len = strlen( mpath );
        h = c_jhash64((uint8_t *) mpath, len, 0);

        /* search that folder in the db and check that the hash is the md5 (etag) is still the same */
        if( csync_get_statedb_exists(ctx) ) {
          tmp = csync_statedb_get_stat_by_hash(ctx, h);
          if (tmp) {
            md5_local = tmp->md5;
            md5_remote = csync_vio_file_id(ctx, uri);

            CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Compare directory ids for %s: %s -> %s", mpath, md5_local, md5_remote );

            if( c_streq(md5_local, md5_remote) ) {
              ctx->remote.read_from_db = 1;
            }
            SAFE_FREE(md5_remote);
            SAFE_FREE(md5_local);
            SAFE_FREE(tmp);
          }
        }

        csync_vio_file_stat_destroy(fs);
    }
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
    goto error;
  }

  /* If remote, compare the id with the local id. If equal, read all contents from
   * the database. */
  read_from_db = ctx->remote.read_from_db;
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "  => Starting to ftw %s, read_from_db-Flag for: %d",
          uri, read_from_db );
  if( ctx->current == REMOTE_REPLICA && !do_read_from_db ) {
      _check_read_from_db(ctx, uri);
      do_read_from_db = (ctx->current == REMOTE_REPLICA && ctx->remote.read_from_db);
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Checking for read from db for %s: %d",
                uri, ctx->remote.read_from_db );

  }

  if ((dh = csync_vio_opendir(ctx, uri)) == NULL) {
    /* permission denied */
    if (errno == EACCES) {
       return 0;
    } else if(errno == EIO ) {
      /* Proxy problems (ownCloud) */
      ctx->error_code = CSYNC_ERR_PROXY;
      goto error;
    } else {
      C_STRERROR(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "opendir failed for %s - %s (errno %d)",
          uri, errbuf, errno);
      ctx->error_code = csync_errno_to_csync_error( CSYNC_ERR_UPDATE );
      goto error;
    }
  }

  while ((dirent = csync_vio_readdir(ctx, dh))) {
    const char *path = NULL;
    int flag;

    d_name = dirent->name;
    if (d_name == NULL) {
      ctx->error_code = CSYNC_ERR_PARAM;
      goto error;
    }

    /* skip "." and ".." */
    if (d_name[0] == '.' && (d_name[1] == '\0'
          || (d_name[1] == '.' && d_name[2] == '\0'))) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      continue;
    }

    if (asprintf(&filename, "%s/%s", uri, d_name) < 0) {
      csync_vio_file_stat_destroy(dirent);
      dirent = NULL;
      ctx->error_code = CSYNC_ERR_PARAM;
      goto error;
    }

    /* Create relative path */
    switch (ctx->current) {
      case LOCAL_REPLICA:
        path = filename + strlen(ctx->local.uri) + 1;
        break;
      case REMOTE_REPLICA:
        path = filename + strlen(ctx->remote.uri) + 1;
        break;
      default:
        break;
    }

    /* skip ".csync_journal.db" and ".csync_journal.db.ctmp" */
    if (c_streq(path, ".csync_journal.db") || c_streq(path, ".csync_journal.db.ctmp")) {
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
        char *md5 = NULL;
        int len = strlen( path );
        uint64_t h = c_jhash64((uint8_t *) path, len, 0);
        md5 = csync_statedb_get_uniqId( ctx, h, fs );
        if( md5 ) {
            SAFE_FREE(fs->md5);
            fs->md5 = md5;
            fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MD5;
        }
        if( c_streq(md5, "")) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database is EMPTY: %s", path);
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Uniq ID from Database: %s -> %s", path, fs->md5 ? fs->md5 : "<NULL>" );
        }
    }

    previous_fs = ctx->current_fs;

    /* Call walker function for each file */
    rc = fn(ctx, filename, fs, flag);

    if (ctx->current_fs && previous_fs && ctx->current_fs->child_modified) {
        previous_fs->child_modified = ctx->current_fs->child_modified;
    }

    if( ! do_read_from_db ) {
        csync_vio_file_stat_destroy(fs);
    } else {
        SAFE_FREE(fs->md5);
    }

    if (rc < 0) {
      csync_vio_closedir(ctx, dh);
      ctx->current_fs = previous_fs;
      goto done;
    }

    if (flag == CSYNC_FTW_FLAG_DIR && depth) {
      rc = csync_ftw(ctx, filename, fn, depth - 1);
      if (rc < 0) {
        ctx->current_fs = previous_fs;
        csync_vio_closedir(ctx, dh);
        goto done;
      }

      if (ctx->current_fs && !ctx->current_fs->child_modified
          && ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL) {
        ctx->current_fs->instruction = CSYNC_INSTRUCTION_NONE;
        ctx->current_fs->should_update_md5 = true;
      }
    }
    ctx->current_fs = previous_fs;
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
  ctx->remote.read_from_db = read_from_db;

  if (dh != NULL) {
    csync_vio_closedir(ctx, dh);
  }
  SAFE_FREE(filename);
  return -1;
}

/* vim: set ts=8 sw=2 et cindent: */
