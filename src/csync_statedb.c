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

#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_statedb.h"
#include "csync_util.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.statedb"
#include "csync_log.h"

#define BUF_SIZE 16

void csync_set_statedb_exists(CSYNC *ctx, int val) {
  ctx->statedb.exists = val;
}

int csync_get_statedb_exists(CSYNC *ctx) {
  return ctx->statedb.exists;
}

static int _csync_statedb_check(const char *statedb) {
  int fd = -1;
  char buf[BUF_SIZE] = {0};
  sqlite3 *db = NULL;

  /* check db version */
#ifdef _WIN32
   _fmode = _O_BINARY;
#endif
  fd = open(statedb, O_RDONLY);
  if (fd >= 0) {
    if (read(fd, (void *) buf, (size_t) BUF_SIZE - 1) >= 0) {
      buf[BUF_SIZE - 1] = '\0';
      close(fd);
      if (c_streq(buf, "SQLite format 3")) {
        if (sqlite3_open(statedb, &db ) == SQLITE_OK) {
          /* everything is fine */
          sqlite3_close(db);
          return 0;
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "database corrupted, removing!");
          unlink(statedb);
        }
        sqlite3_close(db);
      } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite version mismatch");
        unlink(statedb);
      }
    }
  }

  /* create database */
  if (sqlite3_open(statedb, &db) == SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  sqlite3_close(db);

  return -1;
}

static int _csync_statedb_is_empty(CSYNC *ctx) {
  c_strlist_t *result = NULL;
  int rc = 0;

  result = csync_statedb_query(ctx, "SELECT COUNT(phash) FROM metadata LIMIT 1 OFFSET 0;");
  if (result == NULL) {
    rc = 1;
  }
  c_strlist_destroy(result);

  return rc;
}

int csync_statedb_load(CSYNC *ctx, const char *statedb) {
  int rc = -1;
  c_strlist_t *result = NULL;
  char *statedb_tmp = NULL;

  if (_csync_statedb_check(statedb) < 0) {
    rc = -1;
    goto out;
  }

  /*
   * We want a two phase commit for the jounal, so we create a temporary copy
   * of the database.
   * The intention is that if something goes wrong we will not loose the
   * statedb.
   */
  if (asprintf(&statedb_tmp, "%s.ctmp", statedb) < 0) {
    rc = -1;
    goto out;
  }

  if (c_copy(statedb, statedb_tmp, 0644) < 0) {
    rc = -1;
    goto out;
  }

  /* Open the temporary database */
  if (sqlite3_open(statedb_tmp, &ctx->statedb.db) != SQLITE_OK) {
    rc = -1;
    goto out;
  }

  if (_csync_statedb_is_empty(ctx)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "statedb doesn't exist");
    csync_set_statedb_exists(ctx, 0);
  } else {
    csync_set_statedb_exists(ctx, 1);
  }

  /* optimization for speeding up SQLite */
  result = csync_statedb_query(ctx, "PRAGMA default_synchronous = OFF;");
  c_strlist_destroy(result);

  rc = 0;
out:
  SAFE_FREE(statedb_tmp);
  return rc;
}

int csync_statedb_write(CSYNC *ctx) {
  /* drop tables */
  if (csync_statedb_drop_tables(ctx) < 0) {
    return -1;
  }

  /* create tables */
  if (csync_statedb_create_tables(ctx) < 0) {
    return -1;
  }

  /* insert metadata */
  if (csync_statedb_insert_metadata(ctx) < 0) {
    return -1;
  }

  return 0;
}

int csync_statedb_close(CSYNC *ctx, const char *statedb, int jwritten) {
  char *statedb_tmp = NULL;
  int rc = 0;

  /* close the temporary database */
  sqlite3_close(ctx->statedb.db);

  if (asprintf(&statedb_tmp, "%s.ctmp", statedb) < 0) {
    return -1;
  }

  /* if we successfully synchronized, overwrite the original statedb */
  if (jwritten) {
    rc = c_copy(statedb_tmp, statedb, 0644);
    if (rc == 0) {
      unlink(statedb_tmp);
    }
  } else {
    unlink(statedb_tmp);
  }
  SAFE_FREE(statedb_tmp);

  return rc;
}

int csync_statedb_create_tables(CSYNC *ctx) {
  c_strlist_t *result = NULL;

  /*
   * Create temorary table to work on, this speeds up the
   * creation of the statedb.
   */
  result = csync_statedb_query(ctx,
      "CREATE TEMPORARY TABLE IF NOT EXISTS metadata_temp("
      "phash INTEGER(8),"
      "pathlen INTEGER,"
      "path VARCHAR(4096),"
      "inode INTEGER,"
      "uid INTEGER,"
      "gid INTEGER,"
      "mode INTEGER,"
      "modtime INTEGER(8),"
      "md5 VARCHAR(32),"
      "PRIMARY KEY(phash)"
      ");"
      );

  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(ctx,
      "CREATE TABLE IF NOT EXISTS metadata("
      "phash INTEGER(8),"
      "pathlen INTEGER,"
      "path VARCHAR(4096),"
      "inode INTEGER,"
      "uid INTEGER,"
      "gid INTEGER,"
      "mode INTEGER,"
      "modtime INTEGER(8),"
      "md5 VARCHAR(32),"
      "PRIMARY KEY(phash)"
      ");"
      );
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(ctx,
      "CREATE INDEX metadata_phash ON metadata(phash);");
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(ctx,
      "CREATE INDEX metadata_inode ON metadata(inode);");
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(ctx,
      "CREATE INDEX metadata_md5 ON metadata(md5);");
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  return 0;
}

int csync_statedb_drop_tables(CSYNC *ctx) {
  c_strlist_t *result = NULL;

  result = csync_statedb_query(ctx,
      "DROP TABLE IF EXISTS metadata;"
      );
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  return 0;
}

static int _insert_metadata_visitor(void *obj, void *data) {
  csync_file_stat_t *fs = NULL;
  CSYNC *ctx = NULL;
  char *stmt = NULL;
  int rc = -1;

  fs = (csync_file_stat_t *) obj;
  ctx = (CSYNC *) data;

  switch (fs->instruction) {
    /*
     * Don't write ignored, deleted or files with an error to the statedb.
     * They will be visited on the next synchronization again as a new file.
     */
    case CSYNC_INSTRUCTION_DELETED:
    case CSYNC_INSTRUCTION_IGNORE:
    case CSYNC_INSTRUCTION_ERROR:
      rc = 0;
      break;
    case CSYNC_INSTRUCTION_NONE:
    /* As we only sync the local tree we need this flag here */
    case CSYNC_INSTRUCTION_UPDATED:
    case CSYNC_INSTRUCTION_CONFLICT:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE,
        "SQL statement: INSERT INTO metadata_temp \n"
        "\t\t\t(phash, pathlen, path, inode, uid, gid, mode, modtime, md5) VALUES \n"
        "\t\t\t(%llu, %lu, %s, %llu, %u, %u, %u, %lu, %s);",
        (long long unsigned int) fs->phash,
        (long unsigned int) fs->pathlen,
        fs->path,
        (long long unsigned int) fs->inode,
        fs->uid,
        fs->gid,
        fs->mode,
        fs->modtime,
        fs->md5);

      /*
       * The phash needs to be long long unsigned int or it segfaults on PPC
       */
      stmt = sqlite3_mprintf("INSERT INTO metadata_temp "
        "(phash, pathlen, path, inode, uid, gid, mode, modtime, md5) VALUES "
        "(%llu, %lu, '%q', %llu, %u, %u, %u, %lu, '%s');",
        (long long unsigned int) fs->phash,
        (long unsigned int) fs->pathlen,
        fs->path,
        (long long unsigned int) fs->inode,
        fs->uid,
        fs->gid,
        fs->mode,
        fs->modtime,
        fs->md5);

      if (stmt == NULL) {
        return -1;
      }

      rc = csync_statedb_insert(ctx, stmt);

      sqlite3_free(stmt);
      break;
    default:
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN,
          "file: %s, instruction: %s (%d), not added to statedb!",
          fs->path, csync_instruction_str(fs->instruction), fs->instruction);
      rc = 1;
      break;
  }

  return rc;
}

int csync_statedb_insert_metadata(CSYNC *ctx) {
  c_strlist_t *result = NULL;

  if (c_rbtree_walk(ctx->local.tree, ctx, _insert_metadata_visitor) < 0) {
    return -1;
  }

  if (csync_statedb_insert(ctx, "INSERT INTO metadata SELECT * FROM metadata_temp;") < 0) {
    return -1;
  }

  result = csync_statedb_query(ctx, "DROP TABLE metadata_temp;");
  if (result == NULL) {
    return -1;
  }

  c_strlist_destroy(result);

  return 0;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_hash(CSYNC *ctx, uint64_t phash) {
  csync_file_stat_t *st = NULL;
  c_strlist_t *result = NULL;
  char *stmt = NULL;
  size_t len = 0;

  stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE phash='%llu'",
      (long long unsigned int) phash);
  if (stmt == NULL) {
    return NULL;
  }

  result = csync_statedb_query(ctx, stmt);
  sqlite3_free(stmt);
  if (result == NULL) {
    return NULL;
  }

  if (result->count <= 6) {
    c_strlist_destroy(result);
    return NULL;
  }
  /* phash, pathlen, path, inode, uid, gid, mode, modtime */
  len = strlen(result->vector[2]);
  st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
  if (st == NULL) {
    c_strlist_destroy(result);
    return NULL;
  }

  /*
   * FIXME:
   * We use an INTEGER(8) which is signed to the phash in the sqlite3 db,
   * but the phash is an uint64_t. So for some values we get a string like
   * "1.66514565505016e+19". For such a string strtoull() returns 1.
   * phash = 1
   *
   * st->phash = strtoull(result->vector[0], NULL, 10);
   */

   /* The query suceeded so use the phash we pass to the function. */
  st->phash = phash;

  st->pathlen = atoi(result->vector[1]);
  memcpy(st->path, (len ? result->vector[2] : ""), len + 1);
  st->inode = atoi(result->vector[3]);
  st->uid = atoi(result->vector[4]);
  st->gid = atoi(result->vector[5]);
  st->mode = atoi(result->vector[6]);
  st->modtime = strtoul(result->vector[7], NULL, 10);
  st->md5 = c_strdup( result->vector[8] );
  c_strlist_destroy(result);

  return st;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_inode(CSYNC *ctx, uint64_t inode) {
  csync_file_stat_t *st = NULL;
  c_strlist_t *result = NULL;
  char *stmt = NULL;
  size_t len = 0;

  stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE inode='%llu'", inode);
  if (stmt == NULL) {
    return NULL;
  }

  result = csync_statedb_query(ctx, stmt);
  sqlite3_free(stmt);
  if (result == NULL) {
    return NULL;
  }

  if (result->count <= 6) {
    c_strlist_destroy(result);
    return NULL;
  }

  /* phash, pathlen, path, inode, uid, gid, mode, modtime */
  len = strlen(result->vector[2]);
  st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
  if (st == NULL) {
    c_strlist_destroy(result);
    return NULL;
  }

  st->phash = strtoull(result->vector[0], NULL, 10);
  st->pathlen = atoi(result->vector[1]);
  memcpy(st->path, (len ? result->vector[2] : ""), len + 1);
  st->inode = atoi(result->vector[3]);
  st->uid = atoi(result->vector[4]);
  st->gid = atoi(result->vector[5]);
  st->mode = atoi(result->vector[6]);
  st->modtime = strtoul(result->vector[7], NULL, 10);
  st->md5 = c_strdup(result->vector[8]);

  c_strlist_destroy(result);

  return st;
}

/* query the statedb, caller must free the memory */
c_strlist_t *csync_statedb_query(CSYNC *ctx, const char *statement) {
  int err = SQLITE_OK;
  int rc = SQLITE_OK;
  size_t i = 0;
  size_t busy_count = 0;
  size_t retry_count = 0;
  size_t column_count = 0;
  sqlite3_stmt *stmt;
  const char *tail = NULL;
  c_strlist_t *result = NULL;

  do {
    /* compile SQL program into a virtual machine, reattempteing if busy */
    do {
      if (busy_count) {
        /* sleep 100 msec */
        usleep(100000);
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "sqlite3_prepare: BUSY counter: %zu", busy_count);
      }
      err = sqlite3_prepare(ctx->statedb.db, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count ++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite3_compile error: %s - on query %s", sqlite3_errmsg(ctx->statedb.db), statement);
      break;
    } else {
      busy_count = 0;
      column_count = sqlite3_column_count(stmt);

      /* execute virtual machine by iterating over rows */
      for(;;) {
        err = sqlite3_step(stmt);

        if (err == SQLITE_BUSY) {
          if (busy_count++ > 120) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Busy counter has reached its maximum. Aborting this sql statement");
            break;
          }
          /* sleep 100 msec */
          usleep(100000);
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "sqlite3_step: BUSY counter: %zu", busy_count);
          continue;
        }

        if (err == SQLITE_MISUSE) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_step: MISUSE!!");
        }

        if (err == SQLITE_DONE) {
          if (result == NULL) {
            result = c_strlist_new(1);
          }
          break;
        }

        if (err == SQLITE_ERROR) {
          break;
        }

        result = c_strlist_new(column_count);
        if (result == NULL) {
          return NULL;
        }

        /* iterate over columns */
        for (i = 0; i < column_count; i++) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "sqlite3_column_text: %s", (char *) sqlite3_column_text(stmt, i));
          if (c_strlist_add(result, (char *) sqlite3_column_text(stmt, i)) < 0) {
            c_strlist_destroy(result);
            return NULL;
          }
        }
      } /* end infinite for loop */

      /* deallocate vm resources */
      rc = sqlite3_finalize(stmt);

      if (err != SQLITE_DONE && rc != SQLITE_SCHEMA) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s - on query: %s", sqlite3_errmsg(ctx->statedb.db), statement);
        result = c_strlist_new(1);
      }

      if (rc == SQLITE_SCHEMA) {
        retry_count ++;
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "SQLITE_SCHEMA error occurred on query: %s", statement);
        if (retry_count < 10) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Retrying now.");
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "RETRY count has reached its maximum. Aborting statement: %s", statement);
          result = c_strlist_new(1);
        }
      }
    }
  } while (rc == SQLITE_SCHEMA && retry_count < 10);

  return result;
}

int csync_statedb_insert(CSYNC *ctx, const char *statement) {
  int err;
  int rc = 0;
  int busy_count = 0;
  int retry_count = 0;
  sqlite3_stmt *stmt;
  const char *tail;

  if (!statement[0]) {
    return 0;
  }

  do {
    /* compile SQL program into a virtual machine, reattempteing if busy */
    do {
      if (busy_count) {
        /* sleep 100 msec */
        usleep(100000);
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "sqlite3_prepare: BUSY counter: %d", busy_count);
      }
      err = sqlite3_prepare(ctx->statedb.db, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_compile error: %s on query %s", sqlite3_errmsg(ctx->statedb.db), statement);
      break;
    } else {
      busy_count = 0;

      /* execute virtual machine by iterating over rows */
      for(;;) {
        err = sqlite3_step(stmt);

        if (err == SQLITE_BUSY) {
          if (busy_count++ > 120) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Busy counter has reached its maximum. Aborting this sql statement");
            break;
          }
          /* sleep 100 msec */
          usleep(100000);
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "sqlite3_step: BUSY counter: %d", busy_count);
        }

        if (err == SQLITE_MISUSE) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_step: MISUSE!!");
        }

        if (err == SQLITE_DONE || err == SQLITE_ERROR) {
          break;
        }
      } /* end infinite for loop */

      /* deallocate vm resources */
      rc = sqlite3_finalize(stmt);

      if (err != SQLITE_DONE && rc != SQLITE_SCHEMA) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s on insert: %s", sqlite3_errmsg(ctx->statedb.db), statement);
      }

      if (rc == SQLITE_SCHEMA) {
        retry_count++;
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "SQLITE_SCHEMA error occurred on insert: %s", statement);
        if (retry_count < 10) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Retrying now.");
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "RETRY count has reached its maximum. Aborting statement: %s", statement);
        }
      }
    }
  } while (rc == SQLITE_SCHEMA && retry_count < 10);

  return sqlite3_last_insert_rowid(ctx->statedb.db);
}

/* vim: set ts=8 sw=2 et cindent: */
