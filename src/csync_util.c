/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006 by Andreas Schneider <mail@cynapses.org>
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
#include <limits.h>
#include <stdio.h>

#include "c_jhash.h"
#include "csync_util.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.util"
#include "csync_log.h"

typedef struct {
  const char *instr_str;
  enum csync_instructions_e instr_code;
} _instr_code_struct;

static const _instr_code_struct _instr[] =
{
  { "INSTRUCTION_NONE", CSYNC_INSTRUCTION_NONE },
  { "INSTRUCTION_EVAL", CSYNC_INSTRUCTION_EVAL },
  { "INSTRUCTION_REMOVE", CSYNC_INSTRUCTION_REMOVE },
  { "INSTRUCTION_RENAME", CSYNC_INSTRUCTION_RENAME },
  { "INSTRUCTION_NEW", CSYNC_INSTRUCTION_NEW },
  { "INSTRUCTION_CONFLICT", CSYNC_INSTRUCTION_CONFLICT },
  { "INSTRUCTION_IGNORE", CSYNC_INSTRUCTION_IGNORE },
  { "INSTRUCTION_SYNC", CSYNC_INSTRUCTION_SYNC },
  { "INSTRUCTION_STAT_ERR", CSYNC_INSTRUCTION_STAT_ERROR },
  { "INSTRUCTION_ERROR", CSYNC_INSTRUCTION_ERROR },
  { "INSTRUCTION_DELETED", CSYNC_INSTRUCTION_DELETED },
  { "INSTRUCTION_UPDATED", CSYNC_INSTRUCTION_UPDATED },
  { NULL, CSYNC_INSTRUCTION_ERROR }
};

struct csync_memstat_s {
  int size;
  int resident;
  int shared;
  int trs;
  int drs;
  int lrs;
  int dt;
};

const char *csync_instruction_str(enum csync_instructions_e instr)
{
  int idx = 0;

  while (_instr[idx].instr_str != NULL) {
    if (_instr[idx].instr_code == instr) {
      return _instr[idx].instr_str;
    }
    idx++;
  }

  return "ERROR!";
}


void csync_memstat_check(CSYNC *ctx) {
  int s = 0;
  struct csync_memstat_s m;
  FILE* fp;

  /* get process memory stats */
  fp = fopen("/proc/self/statm","r");
  if (fp == NULL) {
    return;
  }
  s = fscanf(fp, "%d%d%d%d%d%d%d", &m.size, &m.resident, &m.shared, &m.trs,
      &m.drs, &m.lrs, &m.dt);
  fclose(fp);
  if (s == EOF) {
    return;
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "Memory: %dK total size, %dK resident, %dK shared",
                 m.size * 4, m.resident * 4, m.shared * 4);
}

static int _merge_file_trees_visitor(void *obj, void *data) {
  csync_file_stat_t *fs = NULL;
  csync_file_stat_t *tfs = NULL;
  csync_vio_file_stat_t *vst = NULL;

  CSYNC *ctx = NULL;
  c_rbtree_t *tree = NULL;
  c_rbnode_t *node = NULL;

  char errbuf[256] = {0};
  char *uri = NULL;
  int rc = -1;

  fs = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  /* search for UPDATED file */
  if (fs->instruction != CSYNC_INSTRUCTION_UPDATED && !fs->should_update_md5) {
    rc = 0;
    goto out;
  }

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

  /* check if the file is new or has been synced */
  node = c_rbtree_find(tree, &fs->phash);
  if (node == NULL) {
    csync_file_stat_t *new_stat = NULL;

    new_stat = c_malloc(sizeof(csync_file_stat_t) + fs->pathlen + 1);
    if (new_stat == NULL) {
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "file: %s, merge malloc, error: %s",
          fs->path,
          errbuf);
      rc = -1;
      goto out;
    }
    new_stat = memcpy(new_stat, fs, sizeof(csync_file_stat_t) + fs->pathlen + 1);
    if (fs->md5)
        new_stat->md5 = c_strdup(fs->md5);
    if (fs->destpath)
        new_stat->destpath = c_strdup(fs->destpath);
    if (fs->error_string)
        new_stat->error_string = c_strdup(fs->error_string);

    if (c_rbtree_insert(tree, new_stat) < 0) {
      strerror_r(errno, errbuf, sizeof(errbuf));
      SAFE_FREE(new_stat);
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "file: %s, rb tree insert, error: %s",
          fs->path,
          errbuf);
      rc = -1;
      goto out;
    }

    node = c_rbtree_find(tree, &fs->phash);
    if (node == NULL) {
      rc = -1;
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Unable to find node");
      goto out;
    }
  }
  fs = c_rbtree_node_data(node);

  switch (ctx->current) {
  case LOCAL_REPLICA:
      /* If there is a destpath, this is a rename and the target must be used. */
      if( fs->destpath ) {
          asprintf(&uri, "%s/%s", ctx->local.uri, fs->destpath);
          SAFE_FREE(fs->destpath);
      } else {
          if (asprintf(&uri, "%s/%s", ctx->local.uri, fs->path) < 0) {
              rc = -1;
              strerror_r(errno, errbuf, sizeof(errbuf));
              CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file uri alloc failed: %s",
                        errbuf);
              goto out;
          }
      }
      break;
    case REMOTE_REPLICA:
      if (asprintf(&uri, "%s/%s", ctx->remote.uri, fs->path) < 0) {
          rc = -1;
          strerror_r(errno, errbuf, sizeof(errbuf));
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "file uri alloc failed: %s",
                    errbuf);
          goto out;
      }
      break;
  default:
      break;
  }

  /* get file stat of the file on the replica */
  vst = csync_vio_file_stat_new();
  if (csync_vio_stat(ctx, uri, vst) < 0) {
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "file: %s, updating stat failed, error: %s",
        uri,
        errbuf);

    /* Not a fatal error, since it is better to have outdated information than no information */
  } else {
    /* update file stat */
    fs->inode = vst->inode;
    fs->modtime = vst->mtime;
  }

  /* update with the id from the remote repo. This method always works on the local repo */
  node = c_rbtree_find(ctx->remote.tree, &fs->phash);
  if (node == NULL) {
      rc = -1;
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Unable to find node");
      goto out;
  }

  tfs = c_rbtree_node_data(node);
  if( tfs && tfs->md5 ) {
      if( fs->md5 ) SAFE_FREE(fs->md5);
      fs->md5 = c_strdup( tfs->md5 );
      CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "PRE UPDATED %s: %s", fs->path, fs->md5);
  } else {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "md5 is empty in merger!");
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "file: %s, instruction: UPDATED (%s)", uri, fs->md5);

  fs->instruction = CSYNC_INSTRUCTION_NONE;

  rc = 0;
out:
  csync_vio_file_stat_destroy(vst);
  SAFE_FREE(uri);

  if (rc != 0) {
    fs->instruction = CSYNC_INSTRUCTION_ERROR;
  }

  return rc;
}

/*
 * merge the local tree with the new files from remote and update the
 * inode numbers
 */
int csync_merge_file_trees(CSYNC *ctx) {
  int rc = -1;

  /* walk over remote tree, stat on local system */
  ctx->current = LOCAL_REPLICA;
  ctx->replica = ctx->local.type;

  rc = c_rbtree_walk(ctx->remote.tree, ctx, _merge_file_trees_visitor);
  if (rc < 0) {
    goto out;
  }

#if 0
  /* We don't have to merge the remote tree atm. */

  /* walk over local tree, stat on remote system */
  ctx->current = REMOTE_REPLICA;
  ctx->replica = ctx->remote.type;

  rc = c_rbtree_walk(ctx->local.tree, ctx, _merge_file_trees_visitor);
  if (rc < 0) {
    goto out;
  }
#endif

out:
  return rc;
}

int csync_unix_extensions(CSYNC *ctx) {
  int rc = -1;
  char *uri = NULL;
  csync_vio_handle_t *fp = NULL;

  ctx->options.unix_extensions = 0;

  rc = asprintf(&uri, "%s/csync_unix_extension*test.ctmp", ctx->remote.uri);
  if (rc < 0) {
    goto out;
  }

  ctx->replica = ctx->remote.type;
  fp = csync_vio_creat(ctx, uri, 0644);
  if (fp == NULL) {
    rc = 0;
    CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO,
        "Disabled unix filesystem synchronization");
    goto out;
  }
  csync_vio_close(ctx, fp);

  ctx->options.unix_extensions = 1;
  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "Enabled unix filesystem synchronization");

  rc = 1;

out:
  csync_vio_unlink(ctx, uri);
  SAFE_FREE(uri);

  return rc;
}


void csync_win32_set_file_hidden( const char *file, bool h ) {
#ifdef _WIN32
  const _TCHAR *fileName;
  if( !file ) return;

  fileName = c_multibyte( file );

  DWORD dwAttrs = GetFileAttributesW(fileName);

  if (dwAttrs==INVALID_FILE_ATTRIBUTES) return;

  if (h && !(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
     SetFileAttributesW(fileName, dwAttrs | FILE_ATTRIBUTE_HIDDEN );
  } else if (!h && (dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
     SetFileAttributesW(fileName, dwAttrs & ~FILE_ATTRIBUTE_HIDDEN );
  }

  c_free_multibyte(fileName);
#else
    (void) h;
    (void) file;
#endif
}
