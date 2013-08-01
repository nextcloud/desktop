/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>wie
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
#include <time.h>

#include "csync_private.h"
#include "csync_misc.h"
#include "csync_propagate.h"
#include "csync_statedb.h"
#include "vio/csync_vio_local.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.propagator"
#include "csync_log.h"
#include "csync_util.h"

static int _csync_cleanup_cmp(const void *a, const void *b) {
  csync_file_stat_t *st_a, *st_b;

  st_a = (csync_file_stat_t *) a;
  st_b = (csync_file_stat_t *) b;

  return strcmp(st_a->path, st_b->path);
}

static bool _push_to_tmp_first(CSYNC *ctx)
{
    if( ctx->current == REMOTE_REPLICA ) return true; /* Always push to tmp for destination local file system */

    /* If destination is the remote replica check if the switch is set. */
    if( !ctx->module.capabilities.atomar_copy_support ) return true;

    return false;
}

static bool _module_supports_put(CSYNC *ctx)
{
    /* If destination is the remote replica check if the switch is set. */
    return ( ctx->module.capabilities.put_support );
}

static bool _module_supports_get(CSYNC *ctx)
{
    /* If destination is the remote replica check if the switch is set. */
    return ( ctx->module.capabilities.get_support );
}

static int _csync_push_file(CSYNC *ctx, csync_file_stat_t *st) {
  enum csync_replica_e srep = -1;
  enum csync_replica_e drep = -1;
  enum csync_replica_e rep_bak = -1;

  char *suri = NULL;
  char *duri = NULL;
  char *turi = NULL;
  char *tdir = NULL;

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

  bool transmission_done = false;

  rep_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      srep = ctx->local.type;
      drep = ctx->remote.type;
      if (asprintf(&suri, "%s/%s", ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }
      break;
    case REMOTE_REPLICA:
      srep = ctx->remote.type;
      drep = ctx->local.type;
      if (asprintf(&suri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }
      if (asprintf(&duri, "%s/%s", ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
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
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
    if (asprintf(&turi, "%s.XXXXXX", duri) < 0) {
      ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
      rc = -1;
      goto out;
    }

    /* We just want a random file name here, open checks if the file exists. */
    if (c_tmpname(turi) < 0) {
      ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
      rc = -1;
      goto out;
    }
  } else {
    /* write to the target file directly as the HTTP server does it atomically */
    if (asprintf(&turi, "%s", duri) < 0) {
      ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
      rc = -1;
      goto out;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
              "Remote repository atomar push enabled for %s.", turi );
  }

  /* Create the destination file */
  ctx->replica = drep;
  while ((dfp = csync_vio_open(ctx, turi, O_CREAT|O_EXCL|O_WRONLY|O_NOCTTY,
          C_FILE_MODE)) == NULL) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
    switch (errno) {
      case EEXIST:
        if (count++ > 10) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
              "file: %s, command: open(O_CREAT), error: max count exceeded",
              duri);
          ctx->status_code = CSYNC_STATUS_OPEN_ERROR;
          rc = 1;
          goto out;
        }
        if(_push_to_tmp_first(ctx)) {
          if (c_tmpname(turi) < 0) {
            ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
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

        if (csync_vio_mkdirs(ctx, tdir, C_DIR_MODE) < 0) {
          ctx->status_code = csync_errno_to_status(errno,
                                                   CSYNC_STATUS_PROPAGATE_ERROR);
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

  /* Check if we have put/get */
  if (_module_supports_put(ctx)) {
    if (srep == ctx->local.type) {
      /* get case: get from remote to a local file descriptor */
      rc = csync_vio_put(ctx, sfp, dfp, st);
      if (rc < 0) {
        ctx->status_code = csync_errno_to_status(errno,
                                                 CSYNC_STATUS_PROPAGATE_ERROR);
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                  "file: %s, command: put, error %s",
                  duri,
                  errbuf);
        rc = 1;
        goto out;
      }
      transmission_done = true;
    }
  }
  if (_module_supports_get(ctx)) {
    if (srep == ctx->remote.type) {
      /* put case: put from a local file descriptor to remote. */
      rc = csync_vio_get(ctx, dfp, sfp, st);
      if (rc < 0) {
        ctx->status_code = csync_errno_to_status(errno,
                                                 CSYNC_STATUS_PROPAGATE_ERROR);
        strerror_r(errno, errbuf, sizeof(errbuf));
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                  "file: %s, command: get, error: %s",
                  duri,
                  errbuf);
        rc = 1;
        goto out;
      }
      transmission_done = true;
    }
  }

  if (!transmission_done) {
    /* no get and put, copy file through own buffers. */
    for (;;) {
      ctx->replica = srep;
      bread = csync_vio_read(ctx, sfp, buf, MAX_XFER_BUF_SIZE);

      if (bread < 0) {
        /* read error */
        ctx->status_code = csync_errno_to_status(errno,
                                                 CSYNC_STATUS_PROPAGATE_ERROR);
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
        ctx->status_code = csync_errno_to_status(errno,
                                                 CSYNC_STATUS_PROPAGATE_ERROR);
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
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
            turi, errbuf);
        break;
    }
  }
  dfp = NULL;

  /*
   * Check filesize
   */
  ctx->replica = drep;
  tstat = csync_vio_file_stat_new();
  if (tstat == NULL) {
    ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
    rc = -1;
    goto out;
  }

  if (csync_vio_stat(ctx, turi, tstat) < 0) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
        "file: %s, error: incorrect filesize (size: %ld should be %ld)",
        turi, (long)tstat->size, (long)st->size);
    ctx->status_code = CSYNC_STATUS_FILE_SIZE_ERROR;
    rc = 1;
    goto out;
  }

  if (_push_to_tmp_first(ctx)) {
    /* override original file */
    ctx->replica = drep;
    if (csync_vio_rename(ctx, turi, duri) < 0) {
      ctx->status_code = csync_errno_to_status(errno,
                                               CSYNC_STATUS_PROPAGATE_ERROR);
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
      ctx->status_code = csync_errno_to_status(errno,
                                               CSYNC_STATUS_PROPAGATE_ERROR);
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

  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_UPDATED;

  /* Notify the overall progress */
  if (ctx->callbacks.overall_progress_cb) {
      ctx->progress.byte_current += st->size;
      ctx->callbacks.overall_progress_cb(duri,
                                         ctx->progress.current_file_no++,
                                         ctx->progress.file_count,
                                         ctx->progress.byte_current,
                                         ctx->progress.byte_sum,
                                         ctx->callbacks.userdata);
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "PUSHED  file: %s", duri);

  rc = 0;

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
      csync_vio_unlink(ctx, turi);
    }
  }

  SAFE_FREE(suri);
  SAFE_FREE(duri);
  SAFE_FREE(turi);
  SAFE_FREE(tdir);

  ctx->replica = rep_bak;

  return rc;
}

static int _backup_path(char** duri, const char* uri, const char* path)
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

    if (asprintf(duri, "%s/%s%s_conflict-%s%s", uri,info->directory ,
                 info->filename,timestring,info->extension) < 0) {
		rc = -1;
	}

	SAFE_FREE(info);
	return rc;
}


static int _csync_backup_file(CSYNC *ctx, csync_file_stat_t *st, char **duri) {
  enum csync_replica_e drep = -1;
  enum csync_replica_e rep_bak = -1;

  char *suri = NULL;

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
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }

      if (_backup_path(duri, ctx->remote.uri,st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }
      break;
    case REMOTE_REPLICA:
      drep = ctx->local.type;
      if (asprintf(&suri, "%s/%s", ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        rc = -1;
        goto out;
      }

      if ( _backup_path(duri, ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
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
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"instruction not allowed: %i %s",
                st->instruction, csync_instruction_str(st->instruction));
      ctx->status_code = CSYNC_STATUS_UNSUCCESSFUL;
      rc = -1;
      goto out;
  }
	
	CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"suri: %s",suri);
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,"duri: %s",*duri);


  /* rename the older file to conflict */
  ctx->replica = drep;
  if (csync_vio_rename(ctx, suri, *duri) < 0) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
        *duri,
        errbuf);
    goto out;
  }


  /* set instruction for the statedb merger */
  st->instruction = CSYNC_INSTRUCTION_NONE;
 
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "BACKUP  file: %s", *duri);

  rc = 0;

out:
  /* set instruction for the statedb merger */
  if (rc != 0) {
    st->instruction = CSYNC_INSTRUCTION_ERROR;
  }

  SAFE_FREE(suri);
 
  ctx->replica = rep_bak;

  return rc;
}

static int _csync_new_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_sync_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;

  rc = _csync_push_file(ctx, st);

  return rc;
}

static int _csync_conflict_file(CSYNC *ctx, csync_file_stat_t *st) {
  int rc = -1;
  char *conflict_file_name;
  char *uri = NULL;

  rc = _csync_backup_file(ctx, st, &conflict_file_name);
  
  if(rc>=0)
  {
	 rc = _csync_push_file(ctx, st);
  }

  if( rc >= 0 ) {
    /* if its the local repository, check if both files are equal. */
    if( ctx->current == REMOTE_REPLICA ) {
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        return -1;
      }

      if( c_compare_file(uri, conflict_file_name) == 1 ) {
        /* the files are byte wise equal. The conflict can be erased. */
        if (csync_vio_local_unlink(conflict_file_name) < 0) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "REMOVE of csync conflict file %s failed.", conflict_file_name );
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "REMOVED csync conflict file %s as files are equal.",
                    conflict_file_name );
        }
      }
    }
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
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    default:
      break;
  }

  if (csync_vio_unlink(ctx, uri) < 0) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    default:
      break;
  }

  ctx->replica = dest;
  if (csync_vio_mkdirs(ctx, uri, C_DIR_MODE) < 0) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
      ctx->status_code = csync_errno_to_status(errno,
                                               CSYNC_STATUS_PROPAGATE_ERROR);
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
  struct timeval times[2];
  int rc = -1;

  replica_bak = ctx->replica;

  switch (ctx->current) {
    case LOCAL_REPLICA:
      dest = ctx->remote.type;
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      dest = ctx->local.type;
      if (asprintf(&uri, "%s/%s", ctx->local.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
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
      ctx->status_code = csync_errno_to_status(errno,
                                               CSYNC_STATUS_PROPAGATE_ERROR);
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
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    case REMOTE_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, st->path) < 0) {
        ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
        return -1;
      }
      break;
    default:
      break;
  }

  if (csync_vio_rmdir(ctx, uri) < 0) {
    ctx->status_code = csync_errno_to_status(errno,
                                             CSYNC_STATUS_PROPAGATE_ERROR);
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
              ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
              return -1;
            }
            ctx->local.list = list;
            break;
          case REMOTE_REPLICA:
            list = c_list_prepend(ctx->remote.list, (void *) st);
            if (list == NULL) {
              ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
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
    ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
    return 0;
  }

  list = c_list_sort(list, _csync_cleanup_cmp);
  if (list == NULL) {
    ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
    return -1;
  }

  for (walk = c_list_last(list); walk != NULL; walk = c_list_prev(walk)) {
    csync_file_stat_t *st = NULL;

    st = (csync_file_stat_t *) walk->data;

    if (asprintf(&dir, "%s/%s", uri, st->path) < 0) {
      ctx->status_code = CSYNC_STATUS_MEMORY_ERROR;
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

static int _csync_propagation_file_count_visitor(void *obj, void *data) {
  csync_file_stat_t *st = NULL;
  CSYNC *ctx = NULL;

  st = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  if (st == NULL) {
      return -1;
  }
  if (ctx == NULL) {
      return -1;
  }

  switch(st->type) {
    case CSYNC_FTW_TYPE_SLINK:
      break;
    case CSYNC_FTW_TYPE_FILE:
      switch (st->instruction) {
        case CSYNC_INSTRUCTION_NEW:
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_CONFLICT:
          ctx->progress.file_count++;
          ctx->progress.byte_sum += st->size;
          break;
        default:
          break;
      }
      break;
    case CSYNC_FTW_TYPE_DIR:
      /*
       * No counting of directories.
       */
      break;
    default:
      break;
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
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_SYNC:
          if (_csync_sync_file(ctx, st) < 0) {
            goto err;
          }
          break;
        case CSYNC_INSTRUCTION_REMOVE:
          if (_csync_remove_file(ctx, st) < 0) {
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

/* Count the files to transmit for both up- and download, ie. in both replicas. */
int csync_init_overall_progress(CSYNC *ctx) {
    int rc;

    if (ctx == NULL) {
        return -1;
    }

    if (ctx->callbacks.overall_progress_cb == NULL) {
        /* No progress callback, no need to count */
        return 0;
    }

    ctx->current = REMOTE_REPLICA;
    ctx->replica = ctx->remote.type;

    rc = c_rbtree_walk(ctx->remote.tree,
                       (void *)ctx,
                       _csync_propagation_file_count_visitor);
    if (rc < 0) {
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
    }
    ctx->current = LOCAL_REPLICA;
    ctx->replica = ctx->local.type;

    rc = c_rbtree_walk(ctx->local.tree,
                       (void *)ctx,
                       _csync_propagation_file_count_visitor);
    if (rc < 0) {
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
        return -1;
    }

    /* Notify the overall progress */
    if (ctx->progress.file_count >0) {
        ctx->progress.current_file_no = 1; /* start with file 1 */
    }
    ctx->callbacks.overall_progress_cb("",
                                       ctx->progress.current_file_no,
                                       ctx->progress.file_count,
                                       ctx->progress.byte_current,
                                       ctx->progress.byte_sum,
                                       ctx->callbacks.userdata);

    return 0;
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

  return 0;
}
