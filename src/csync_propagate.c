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
#include <time.h>
#include <limits.h>

#include "csync_private.h"
#include "csync_propagate.h"
#include "vio/csync_vio.h"
#include "c_jhash.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.propagator"
#include "csync_log.h"
#include "csync_util.h"

static int _csync_cleanup_cmp(const void *a, const void *b) {
  csync_file_stat_t *st_a, *st_b;

  st_a = (csync_file_stat_t *) a;
  st_b = (csync_file_stat_t *) b;

  return strcmp(st_a->path, st_b->path);
}

static void _store_id_update(CSYNC *ctx, csync_file_stat_t *st) {
    c_list_t *list = NULL;
    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "SYNCED remember  dir: %s", st->path);

    switch (ctx->current) {
      case LOCAL_REPLICA:
        list = c_list_prepend(ctx->local.id_list, (void*)st);
        if( list != NULL ) {
            ctx->local.id_list = list;
        }
        break;
      case REMOTE_REPLICA:
        list = c_list_prepend(ctx->remote.id_list, (void*)st);
        if(list != NULL ) {
            ctx->remote.id_list = list;
        }
        break;

    }
}

static bool _push_to_tmp_first(CSYNC *ctx)
{
    if( !ctx ) return true;
    if( ctx->current == REMOTE_REPLICA ) return true; /* Always push to tmp for destination local file system */

    /* If destination is the remote replica check if the switch is set. */
    if( !ctx->module.capabilities.atomar_copy_support ) return true;

    return false;
}

static bool _use_fd_based_push(CSYNC *ctx)
{
    if(!ctx) return false;

    if( ctx->module.capabilities.use_send_file_to_propagate ) return true;
    return false;
}

static const char*_get_md5( CSYNC *ctx, const char *path ) {
  const char *md5 = NULL;
  char *buf = NULL;

  /* Always use the remote uri path, local does not have Ids. */
  if (asprintf(&buf, "%s/%s", ctx->remote.uri, path) < 0) {
      return 0;
  }

  md5 = csync_vio_file_id(ctx, buf);

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "MD5 for %s: %s", buf, md5 ? md5 : "<null>");
  SAFE_FREE(buf);
  return md5;
}

static int _csync_push_file(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e srep = -1;
  enum csync_replica_e drep = -1;
  enum csync_replica_e rep_bak = -1;

  char *suri = NULL;
  char *duri = NULL;
  char *turi = NULL;
  char *tdir = NULL;
  const char *tmd5 = NULL;
  char *prev_tdir  = NULL;

  csync_vio_handle_t *sfp = NULL;
  csync_vio_handle_t *dfp = NULL;

  csync_vio_file_stat_t *tstat = NULL;

  char errbuf[256] = {0};
  char buf[MAX_XFER_BUF_SIZE] = {0};
  ssize_t bread = 0;
  ssize_t bwritten = 0;
  struct timeval times[2];

  int rc = -1;
  int count = 0;
  int flags = 0;

  rep_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      srep = ctx->local.type;
      drep = ctx->remote.type;
      if (asprintf(&suri, "%s/%s", ctx->local.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      break;
    case REMOTE_REPLICA:
      srep = ctx->remote.type;
      drep = ctx->local.type;
      if (asprintf(&suri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->local.uri, st->path) < 0) {
        rc = -1;
        goto out;
      }
      break;
    default:
      break;
  }

  /* Open the source file */
  ctx->replica = srep;
  flags = O_RDONLY|O_NOFOLLOW;
#ifdef O_NOATIME
  /* O_NOATIME can only be set by the owner of the file or the superuser */
  if (st->uid == ctx->pwd.uid || ctx->pwd.euid == 0) {
    flags |= O_NOATIME;
  }
#endif
  sfp = csync_vio_open(ctx, suri, flags, 0);
  if (sfp == NULL) {
    if (errno == ENOMEM) {
      rc = -1;
    } else {
      rc = 1;
    }

    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "file: %s, command: open(O_RDONLY), error: %s",
        suri, errbuf );

    goto out;
  }

  if (_push_to_tmp_first(ctx)) {
      /* create the temporary file name */
#ifdef _WIN32
      if (asprintf(&turi, "%s.~XXXXXX", duri) < 0) {
#else
      /* split up the path */
      int re = 0;
      if(duri) {
          char *path = c_dirname(duri);
          char *base = c_basename(duri);

          if( path ) {
              re = asprintf(&turi, "%s/.%s.~XXXXXX", path, base);
          } else {
              re = asprintf(&turi,".%s.~XXXXXX", base);
          }
          SAFE_FREE(path);
          SAFE_FREE(base);
      }
      if (re < 0) {
#endif
          rc = -1;
          goto out;
      }

      /* We just want a random file name here, open checks if the file exists. */
      if (c_tmpname(turi) < 0) {
          rc = -1;
          goto out;
      }
  } else {
      /* write to the target file directly as the HTTP server does it atomically */
      if (asprintf(&turi, "%s", duri) < 0) {
          rc = -1;
          goto out;
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
                "Remote repository atomar push enabled for %s (%d).", turi, ctx->current);

  }

  /* Create the destination file */
  ctx->replica = drep;
  while ((dfp = csync_vio_open(ctx, turi, O_CREAT|O_EXCL|O_WRONLY|O_NOCTTY,
          C_FILE_MODE)) == NULL) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
          "file: %s, command: open(O_CREAT), error: %d",
          duri, errno);

    switch (errno) {
      case EEXIST:
        if (count++ > 10) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
              "file: %s, command: open(O_CREAT), error: max count exceeded",
              duri);
          rc = 1;
          goto out;
        }
        if(_push_to_tmp_first(ctx)) {
          if (c_tmpname(turi) < 0) {
            rc = -1;
            goto out;
          }
        }
        break;
      case ENOENT:
        /* get the directory name */
        tdir = c_dirname(turi);
        if (tdir == NULL) {
          rc = -1;
          goto out;
        }

        if( prev_tdir && c_streq(tdir, prev_tdir) ) {
            /* we're looping */
            CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN,
                      "dir: %s, loop in mkdir detected!", tdir);
            rc = 0;
            st->instruction = CSYNC_INSTRUCTION_IGNORE;
            goto out;
        }
        SAFE_FREE(prev_tdir);
        prev_tdir = c_strdup(tdir);

        if (csync_vio_mkdirs(ctx, tdir, C_DIR_MODE) < 0) {
          strerror_r(errno, errbuf, sizeof(errbuf));
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN,
              "dir: %s, command: mkdirs, error: %s",
              tdir, errbuf);
        }
        break;
      case ENOMEM:
        rc = -1;
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
            "file: %s, command: open(O_CREAT), error: %s",
            turi, errbuf);
        goto out;
        break;
      default:
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
            "file: %s, command: open(O_CREAT), error: %s",
            turi, errbuf);
        rc = 1;
        goto out;
        break;
    }

  }

  /* copy file */
  if( _use_fd_based_push(ctx) ) {
      if (ctx->current == REMOTE_REPLICA)
	    csync_win32_set_file_hidden(turi, true);

      rc = csync_vio_sendfile( ctx, sfp, dfp );

	  if (ctx->current == REMOTE_REPLICA)
	    csync_win32_set_file_hidden(turi, false);

      if( rc != 0 ) {
          strerror_r(errno,  errbuf, sizeof(errbuf));
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                    "file: %s, command: sendfile, error: %s from errno %d",
                    suri, errbuf, errno);
          goto out;
      }
  } else {
      for (;;) {
          ctx->replica = srep;
          bread = csync_vio_read(ctx, sfp, buf, MAX_XFER_BUF_SIZE);

          if (bread < 0) {
              /* read error */
              strerror_r(errno,  errbuf, sizeof(errbuf));
              CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                        "file: %s, command: read, error: %s",
                        suri, errbuf);
              rc = 1;
              goto out;
          } else if (bread == 0) {
              /* done */
              break;
          }

          ctx->replica = drep;
          bwritten = csync_vio_write(ctx, dfp, buf, bread);

          if (bwritten < 0 || bread != bwritten) {
              strerror_r(errno, errbuf, sizeof(errbuf));
              CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                        "file: %s, command: write, error: bread = %zu, bwritten = %zu - %s",
                        duri,
                        bread,
                        bwritten,
                        errbuf);
              rc = 1;
              goto out;
          }
      }
  }
  ctx->replica = srep;
  if (csync_vio_close(ctx, sfp) < 0) {
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "file: %s, command: close, error: %s",
        suri,
        errbuf);
  }
  sfp = NULL;

  ctx->replica = drep;
  if (csync_vio_close(ctx, dfp) < 0) {
    dfp = NULL;
    switch (errno) {
    /* stop if no space left or quota exceeded */
    case ENOSPC:
    case EDQUOT:
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                "file: %s, command: close, error: %s",
                turi,
                errbuf);
      rc = -1;
      goto out;
      break;
    default:
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                "file: %s, command: close, error: %s",
                turi,
                errbuf);
      break;
    }
  }
  dfp = NULL;

  if( ctx->module.capabilities.do_post_copy_stat ) {
    /*
     * Check filesize
     * In case the transport is secure and/or the stat is expensive, this check
     * could be skipped through module capabilities definitions.
     */

    ctx->replica = drep;
    tstat = csync_vio_file_stat_new();
    if (tstat == NULL) {
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                "file: %s, command: stat, error: %s",
                turi,
                errbuf);
      rc = -1;
      goto out;
    }

    if (csync_vio_stat(ctx, turi, tstat) < 0) {
      switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
      }
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                "file: %s, command: stat, error: %s",
                turi,
                errbuf);
      goto out;
    }

    if (st->size != tstat->size) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                "file: %s, error: incorrect filesize (size: %jd should be %jd)",
                turi, tstat->size, st->size);
      rc = 1;
      goto out;
    }

    if( st->md5 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "UUUU MD5 sum: %s", st->md5);
    } else {
      if( tstat->md5 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Target MD5 sum is %s", tstat->md5 );
        if(st->md5) SAFE_FREE(st->md5);
        st->md5 = c_strdup(tstat->md5 );
      } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "MD5 sum is empty");
      }
    }
  }

  if (_push_to_tmp_first(ctx)) {
    /* override original file */
    ctx->replica = drep;
    if (csync_vio_rename(ctx, turi, duri) < 0) {
      switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
      }
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                 "file: %s, command: rename, error: %s",
                  duri,
                  errbuf);
       goto out;
    }
  }
  /* set mode only if it is not the default mode */
  if ((st->mode & 07777) != C_FILE_MODE) {
    if (csync_vio_chmod(ctx, duri, st->mode) < 0) {
      switch (errno) {
        case ENOMEM:
          rc = -1;
          break;
        default:
          rc = 1;
          break;
      }
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "file: %s, command: chmod, error: %s",
          duri,
          errbuf);
      goto out;
    }
  }

  /* set owner and group if possible */
  if (ctx->pwd.euid == 0) {
    csync_vio_chown(ctx, duri, st->uid, st->gid);
  }

  /* sync time */
  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  ctx->replica = drep;
  csync_vio_utimes(ctx, duri, times);


  /* For remote repos, after the utimes call, the ID has changed again */
  /* do a stat on the target again to get a valid md5 */
  tmd5 = _get_md5(ctx, st->path);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "FINAL MD5: %s", tmd5 ? tmd5 : "<null>");

  if(tmd5) {
      SAFE_FREE(st->md5);
      st->md5 = tmd5;
  }

  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_UPDATED;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "PUSHED  file: %s", duri);

  rc = 0;
  if (ctx->current == LOCAL_REPLICA) {
    _store_id_update(ctx, st);
  }

out:
  ctx->replica = srep;
  csync_vio_close(ctx, sfp);

  ctx->replica = drep;
  csync_vio_close(ctx, dfp);

  csync_vio_file_stat_destroy(tstat);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_ERROR;
    if (turi != NULL) {
      /* FIXME: Think again if unlink makes sense. It does not for ownCloud */
      /* csync_vio_unlink(ctx, turi); *
       */
    }
  }

  SAFE_FREE(prev_tdir);
  SAFE_FREE(suri);
  SAFE_FREE(duri);
  SAFE_FREE(turi);
  SAFE_FREE(tdir);

  ctx->replica = rep_bak;

  return rc;
}

static int _backup_path(CSYNC *ctx, char** duri, const char* uri, const char* path)
{
	int rc=0;
	C_PATHINFO *info=NULL;

	struct tm *curtime;
	time_t sec;
	char timestring[16];
	time(&sec);
	curtime = localtime(&sec);
	strftime(timestring, 16,   "%Y%m%d-%H%M%S",curtime);

	info=c_split_path(path);
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"directory: %s",info->directory);
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"filename : %s",info->filename);
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"extension: %s",info->extension);

	if (asprintf(duri, "%s/%s%s_conflict-%s%s", uri,info->directory ,info->filename,timestring,info->extension) < 0) {
		rc = -1;
	}

	SAFE_FREE(info);
	return rc;
}


static int _csync_backup_file(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e drep = -1;
  enum csync_replica_e rep_bak = -1;

  char *suri = NULL;
  char *duri = NULL;

  char errbuf[256] = {0};

  int rc = -1;

  rep_bak = ctx->replica;
  
  if(st->instruction==CSYNC_INSTRUCTION_CONFLICT)
  {
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"CSYNC_INSTRUCTION_CONFLICT");
	switch (ctx->current) {
		case LOCAL_REPLICA:
		drep = ctx->remote.type;
		if (asprintf(&suri, "%s/%s", ctx->remote.uri, st->path) < 0) {
			rc = -1;
			goto out;
		}

		if (_backup_path(ctx, &duri, ctx->remote.uri,st->path) < 0) {
			rc = -1;
			goto out;
		}
		break;
		case REMOTE_REPLICA:
		drep = ctx->local.type;
		if (asprintf(&suri, "%s/%s", ctx->local.uri, st->path) < 0) {
			rc = -1;
			goto out;
		}

		if ( _backup_path(ctx, &duri, ctx->local.uri, st->path) < 0) {
			rc = -1;
			goto out;
		}
		break;
		default:
		break;
	}
  }

  else
  {
	  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"instruction not allowed: %i %s",st->instruction,csync_instruction_str(st->instruction));
	  rc = -1;
      goto out;
  }
	
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"suri: %s",suri);
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"duri: %s",duri);


  /* rename the older file to conflict */
  ctx->replica = drep;
  if (csync_vio_rename(ctx, suri, duri) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "file: %s, command: rename, error: %s",
        duri,
        errbuf);
    goto out;
  }


  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_NONE;
 
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "BACKUP  file: %s", duri);

  rc = 0;

out:
  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_ERROR;
  }

  SAFE_FREE(suri);
  SAFE_FREE(duri);
 
  ctx->replica = rep_bak;

  return rc;
}

static int _csync_new_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_rename_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = 0;
  char errbuf[256] = {0};
  struct timeval times[2];
  char *suri = NULL;
  char *duri = NULL;
  const char *tmd5 = NULL;
  c_rbnode_t *node = NULL;

  switch (ctx->current) {
    case REMOTE_REPLICA:
      if( !(st->path && st->destpath) ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Rename failed: src or dest path empty");
          rc = -1;
      }
      if (asprintf(&suri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        rc = -1;
      }
      if (asprintf(&duri, "%s/%s", ctx->remote.uri, st->destpath) < 0) {
        rc = -1;
      }
      break;
    case LOCAL_REPLICA:
      /* No renaming supported by updater */
      CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "RENAME is only supported on local filesystem.");
      rc = -1;
      goto out;
      break;
    default:
      break;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Renaming %s => %s", suri, duri);

  if (rc > -1 && csync_vio_rename(ctx, suri, duri) < 0) {
    switch (errno) {
      default:
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
            "dir: %s, command: rename, error: %s",
            suri,
            errbuf);
        rc = -1;
        break;
    }
    goto out;
  }

  /* set owner and group if possible */
  if (ctx->pwd.euid == 0) {
    csync_vio_chown(ctx, duri, st->uid, st->gid);
  }

  /* sync time */
  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, duri, times);

  /* The the uniq ID for the destination */
  tmd5 = _get_md5(ctx, st->destpath);

  if( rc > -1 ) {
      /* Find the destination entry in the local tree and insert the uniq id */
      int len = strlen(st->destpath);
      uint64_t h = c_jhash64((uint8_t *) st->destpath, len, 0);
      h = c_jhash64((uint8_t *) st->destpath, len, 0);

      /* search in the local tree for the local file to get the mtime */
      node =  c_rbtree_find(ctx->local.tree, &h);
      if(node == NULL) {
          /* no local file found. */

      } else {
          csync_file_stat_t *other = NULL;
          /* set the mtime which is needed in statedb_get_uniqid */
          other = (csync_file_stat_t *) node->data;
          if( other ) {
              other->md5 = tmd5;
          }
      }
      /* set instruction for the statedb merger */
      st->instruction = CSYNC_INSTRUCTION_DELETED;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "RENAME  file: %s => %s with ID %s", st->path, st->destpath, st->md5);

  if (ctx->current == REMOTE_REPLICA) {
      _store_id_update(ctx, st);
  }

out:
  SAFE_FREE(suri);
  SAFE_FREE(duri);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_NONE;
  }

  return rc;
}

static int _csync_sync_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_conflict_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;
 
  rc = _csync_backup_file(ctx, st);
  
  if(rc>=0)
  {
	 rc = _csync_push_file(ctx, st);
  }

  return rc;
}

static int _csync_remove_file(CSYNC *ctx, csync_file_stat_t *st) {
  char errbuf[256] = {0};
  char *uri = NULL;
  int rc = -1;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  if (csync_vio_unlink(ctx, uri) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "file: %s, command: unlink, error: %s",
        uri,
        errbuf);
    goto out;
  }

  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_DELETED;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "REMOVED file: %s", uri);

  if (ctx->current == REMOTE_REPLICA) {
      _store_id_update(ctx, st);
  }
  

  rc = 0;
out:
  SAFE_FREE(uri);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    /* Write file to statedb, to try to sync again on the next run. */
    st->instruction = CSYNC_INSTRUCTION_NONE;
  }

  return rc;
}

static int _csync_new_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e dest = -1;
  enum csync_replica_e replica_bak;
  char errbuf[256] = {0};
  char *uri = NULL;
  const char *tmd5 = NULL;
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;
  if (csync_vio_mkdirs(ctx, uri, C_DIR_MODE) < 0) {
    switch (errno) {
      case ENOMEM:
        rc = -1;
        break;
      default:
        rc = 1;
        break;
    }
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "dir: %s, command: mkdirs, error: %s",
        uri,
        errbuf);
    goto out;
  }

  /* chmod is if it is not the default mode */
  if ((st->mode & 07777) != C_DIR_MODE) {
    if (csync_vio_chmod(ctx, uri, st->mode) < 0) {
      switch (errno) {
        case ENOMEM:
          rc = -1;
          break;
        default:
          rc = 1;
          break;
      }
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "dir: %s, command: chmod, error: %s",
          uri,
          errbuf);
      goto out;
    }
  }

  /* set owner and group if possible */
  if (ctx->pwd.euid == 0) {
    csync_vio_chown(ctx, uri, st->uid, st->gid);
  }

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, uri, times);

  if (ctx->replica == REMOTE_REPLICA) {
    tmd5 = _get_md5(ctx, st->path);
    if(tmd5) {
      SAFE_FREE(st->md5);
      st->md5 = tmd5;
    }
  }

  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_UPDATED;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "CREATED  dir: %s", uri);
  ctx->replica = replica_bak;

  rc = 0;
out:
  SAFE_FREE(uri);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_ERROR;
  }

  return rc;
}

static int _csync_sync_dir(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e dest = -1;
  enum csync_replica_e replica_bak;
  char errbuf[256] = {0};
  char *uri = NULL;
  const char *tmd5 = NULL;
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;

  /* chmod is if it is not the default mode */
  if ((st->mode & 07777) != C_DIR_MODE) {
    if (csync_vio_chmod(ctx, uri, st->mode) < 0) {
      switch (errno) {
        case ENOMEM:
          rc = -1;
          break;
        default:
          rc = 1;
          break;
      }
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "dir: %s, command: chmod, error: %s",
          uri,
          errbuf);
      goto out;
    }
  }

  /* set owner and group if possible */
  if (ctx->pwd.euid == 0) {
    csync_vio_chown(ctx, uri, st->uid, st->gid);
  }

  times[0].tv_sec = times[1].tv_sec = st->modtime;
  times[0].tv_usec = times[1].tv_usec = 0;

  csync_vio_utimes(ctx, uri, times);
  if (ctx->replica == REMOTE_REPLICA) {
    tmd5 = _get_md5(ctx, st->path);
    if(tmd5) {
        SAFE_FREE(st->md5);
        st->md5 = tmd5;
    }
  }
  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_UPDATED;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "SYNCED   dir: %s", uri);

  ctx->replica = replica_bak;

  rc = 0;
out:
  SAFE_FREE(uri);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_ERROR;
  }

  return rc;
}

static int _csync_remove_dir(CSYNC *ctx, csync_file_stat_t *st) {
  c_list_t *list = NULL;
  char errbuf[256] = {0};
  char *uri = NULL;
  int rc = -1;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        return -1;
      }
      break;
    default:
      break;
  }

  if (csync_vio_rmdir(ctx, uri) < 0) {
    switch (errno) {
      case ENOMEM:
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL,
            "dir: %s, command: rmdir, error: %s",
            uri,
            errbuf);
        rc = -1;
        break;
      case ENOTEMPTY:
        switch (ctx->current) {
          case LOCAL_REPLICA:
            list = c_list_prepend(ctx->local.list, (void *) st);
            if (list == NULL) {
              return -1;
            }
            ctx->local.list = list;
            break;
          case REMOTE_REPLICA:
            list = c_list_prepend(ctx->remote.list, (void *) st);
            if (list == NULL) {
              return -1;
            }
            ctx->remote.list = list;
            break;
          default:
            break;
        }
        rc = 0;
        break;
      default:
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
            "dir: %s, command: rmdir, error: %s",
            uri,
            errbuf);
        rc = 1;
        break;
    }
    goto out;
  }

  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_DELETED;

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "REMOVED  dir: %s", uri);

  rc = 0;
out:
  SAFE_FREE(uri);

  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_NONE;
  }

  return rc;
}

static int _cmp_char( const void *d1, const void *d2 )
{
    const char *c1 = (const char*) d1;
    const char *c2 = (const char*) d2;
    // CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "COMPARE: %s <-> %s", c1, c2);
    if( c_streq(c1, c2) ) return 0;
    return 1;
}


/* Internal helper method for _csync_correct_id */
static void _csync_correct_id_helper(CSYNC *ctx, char *path, c_list_t **seen_dirs, c_rbtree_t *tree)
{
    while( path ) {
        uint64_t h;
        int len;
        c_rbnode_t *node = NULL;

        char *old_path = path;
        csync_file_stat_t *tfs = NULL;

        /* do stuff with the dir here */

        if( *seen_dirs && c_list_find_custom( *seen_dirs,  path, _cmp_char)) {
            // CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "saw this dir already: %s", path);
        } else {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "climb on dir: %s", path);
            *seen_dirs = c_list_prepend( *seen_dirs, c_strdup(path));

            /* Find the correct target entry. */
            len = strlen(path);
            h = c_jhash64((uint8_t *) path, len, 0);

            node = c_rbtree_find(tree, &h);
            if (node == NULL) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Unable to find node");
            } else {
                tfs = c_rbtree_node_data(node);
                if( tfs ) {
                    if(tfs->instruction == CSYNC_INSTRUCTION_DELETED) {
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Skipping update of MD5 because item is deleted.");
                    } else {
                        if(tfs->md5) SAFE_FREE(tfs->md5);
                                tfs->md5 = _get_md5(ctx, path);
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "MD5 for dir: %s %s (Instruction: %s)", tfs->path,
                                tfs->md5, csync_instruction_str(tfs->instruction));
                        if( tfs->md5 && tfs->instruction == CSYNC_INSTRUCTION_NONE ) {
                            /* set instruction for the statedb merger */
                            tfs->instruction = CSYNC_INSTRUCTION_UPDATED;
                        }
                    }
                }
            }
        }
        /* get the parent dir */
        path = c_dirname( path );
        /* free the old path memory */
        SAFE_FREE(old_path );

        /* exit on top directory */
        if( c_streq(path, ".")) {
            SAFE_FREE(path);
            path = NULL;
        }
    }
}


/*
 * This function corrects the unique IDs of parent directories of changed
 * files. Other than in the file system, the change of a unique Id propagates
 * up to the top dir. To save the correct IDs, in all propagations, pathes
 * are recorded in the local and remotes id_list lists.
 * In this function, the unique ID is queried for each directory once and
 * stored into the according entry.
 */
static int _csync_correct_id(CSYNC *ctx) {
    c_list_t *walk      = NULL;
    c_list_t *seen_dirs = NULL;
    c_list_t *list      = NULL;
    c_rbtree_t *tree    = NULL;
    const char *replica = NULL;
    char *path          = NULL;

    switch (ctx->current) {
      case LOCAL_REPLICA:
        list = ctx->local.id_list;
        tree = ctx->local.tree;
        replica = "LOCAL_REPLICA";
        break;
      case REMOTE_REPLICA:
        list = ctx->remote.id_list;
        tree = ctx->remote.tree;
        replica = "REMOTE_REPLICA";
        break;
      default:
        break;
    }

    if (list == NULL) {
      return 0;
    }

    list = c_list_sort(list, _csync_cleanup_cmp);
    if (list == NULL) {
      return -1;
    }

    for (walk = c_list_last(list); walk != NULL; walk = c_list_prev(walk)) {
      csync_file_stat_t *st = NULL;

      st = (csync_file_stat_t *) walk->data;
      if( st->type == CSYNC_FTW_TYPE_FILE ) {
          path = c_dirname( st->path );
      } else if( st->type == CSYNC_FTW_TYPE_DIR ) {
          path = c_strdup(st->path); /* Allocate mem for this not to disappoint FREE */
      } else {
          /* unhandled path */
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "correct ID on dir: %s", path);

      /* handle the . path */
      if( path && path[0] == '.' && strlen(path) == 1) {
          SAFE_FREE(path);
          path = NULL;
      }

      _csync_correct_id_helper(ctx, path, &seen_dirs, tree);

      if (st->type == CSYNC_FTW_TYPE_FILE && ctx->current == REMOTE_REPLICA
          && st->destpath) {
          path = c_dirname( st->destpath );
          if( path && path[0] == '.' && strlen(path) == 1) {
              SAFE_FREE(path);
              path = NULL;
          }
          _csync_correct_id_helper(ctx, path, &seen_dirs, ctx->local.tree);
      }

    }

    /* Free the seen_dirs list */

    if( seen_dirs ) {
        c_list_t *walk1 = NULL;

        for (walk1 = c_list_first(seen_dirs); walk1 != NULL; walk1 = c_list_next(walk1)) {
            char *data = NULL;

            data = (char*) walk1->data;
            SAFE_FREE(data);
        }
    }
    c_list_free(seen_dirs);
    return 0;
}

static int _csync_propagation_cleanup(CSYNC *ctx) {
  c_list_t *list = NULL;
  c_list_t *walk = NULL;
  char *uri = NULL;
  char *dir = NULL;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      list = ctx->local.list;
      uri = ctx->local.uri;
      break;
    case REMOTE_REPLICA:
      list = ctx->remote.list;
      uri = ctx->remote.uri;
      break;
    default:
      break;
  }

  if (list == NULL) {
    return 0;
  }

  list = c_list_sort(list, _csync_cleanup_cmp);
  if (list == NULL) {
    return -1;
  }

  for (walk = c_list_last(list); walk != NULL; walk = c_list_prev(walk)) {
    csync_file_stat_t *st = NULL;

    st = (csync_file_stat_t *) walk->data;

    if (asprintf(&dir, "%s/%s", uri, st->path) < 0) {
      return -1;
    }

    if (csync_vio_rmdir(ctx, dir) < 0) {
      /* Write it back to statedb, that we try to delete it next time. */
      st->instruction = CSYNC_INSTRUCTION_NONE;
    } else {
      st->instruction = CSYNC_INSTRUCTION_DELETED;
    }

    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "CLEANUP  dir: %s", dir);

    SAFE_FREE(dir);
  }

  return 0;
}

static int _csync_propagation_file_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch(st->type) {
    case CSYNC_FTW_TYPE_SLINK:
      break;
    case CSYNC_FTW_TYPE_FILE:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          if (_csync_new_file(ctx, st) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"FAIL NEW: %s",st->path);
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_RENAME:
          if (_csync_rename_file(ctx, st) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"FAIL RENAME: %s",st->path);
            goto err;
          }
          break;
      case CSYNC_INSTRUCTION_SYNC:
          if (_csync_sync_file(ctx, st) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"FAIL SYNC: %s",st->path);
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          if (_csync_remove_file(ctx, st) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"FAIL REMOVE: %s",st->path);
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_CONFLICT:
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"case CSYNC_INSTRUCTION_CONFLICT: %s",st->path);
          if (_csync_conflict_file(ctx, st) < 0) {
            goto err;
          }
          break;
        default:
          break;
      }
      break;
    case CSYNC_FTW_TYPE_DIR:
      /*
       * We have to walk over the files first. If you create or rename a file
       * in a directory on unix. The modification time of the directory gets
       * changed.
       */
      break;
    default:
      break;
  }

  return 0;
err:
  return -1;
}

static int _csync_propagation_dir_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch(st->type) {
    case CSYNC_FTW_TYPE_SLINK:
      /* FIXME: implement symlink support */
      break;
    case CSYNC_FTW_TYPE_FILE:
      break;
    case CSYNC_FTW_TYPE_DIR:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
          if (_csync_new_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_SYNC:
          if (_csync_sync_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_CONFLICT:
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"directory attributes different");
          if (_csync_sync_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          if (_csync_remove_dir(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_RENAME:
          /* there can't be a rename for dirs. See updater. */
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return 0;
err:
  return -1;
}

int csync_propagate_files(CSYNC *ctx) {
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

  if (c_rbtree_walk(tree, (void *) ctx, _csync_propagation_file_visitor) < 0) {
    return -1;
  }

  if (c_rbtree_walk(tree, (void *) ctx, _csync_propagation_dir_visitor) < 0) {
    return -1;
  }

  if (_csync_propagation_cleanup(ctx) < 0) {
    return -1;
  }

  if( _csync_correct_id(ctx) < 0) {
    return -1;
  }
  return 0;
}

/* vim: set ts=8 sw=2 et cindent: */
