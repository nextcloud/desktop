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

#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_statedb.h"
#include "csync_util.h"
#include "csync_misc.h"

#include "c_string.h"
#include "c_jhash.h"
#include "csync_time.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.statedb"
#include "csync_log.h"
#include "csync_rename.h"

#define BUF_SIZE 16

void csync_set_statedb_exists(CSYNC *ctx, int val) {
  ctx->statedb.exists = val;
}

int csync_get_statedb_exists(CSYNC *ctx) {
  return ctx->statedb.exists;
}

static int _csync_check_db_integrity(sqlite3 *db) {
    c_strlist_t *result = NULL;
    int rc = -1;

    result = csync_statedb_query(db, "PRAGMA quick_check;");
    if (result != NULL) {
        /* There is  a result */
        if (result->count > 0) {
            if (c_streq(result->vector[0], "ok")) {
                rc = 0;
            }
        }
        c_strlist_destroy(result);
    }

    return rc;

}

static int _csync_statedb_check(const char *statedb) {
  int fd = -1, rc;
  ssize_t r;
  char buf[BUF_SIZE] = {0};
  sqlite3 *db = NULL;
  csync_stat_t sb;

  mbchar_t *wstatedb = c_utf8_to_locale(statedb);

  if (wstatedb == NULL) {
    return -1;
  }

  /* check db version */
#ifdef _WIN32
    _fmode = _O_BINARY;
#endif

    fd = _topen(wstatedb, O_RDONLY);

    if (fd >= 0) {
        /* Check size. Size of zero is a valid database actually. */
        rc = _tfstat(fd, &sb);

        if (rc == 0) {
            if (sb.st_size == 0) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Database size is zero byte!");
                close(fd);
            } else {
                r = read(fd, (void *) buf, sizeof(buf) - 1);
                close(fd);
                if (r >= 0) {
                    buf[BUF_SIZE - 1] = '\0';
                    if (c_streq(buf, "SQLite format 3")) {
                        if (sqlite3_open(statedb, &db ) == SQLITE_OK) {
                            rc = _csync_check_db_integrity(db);
                            if( sqlite3_close(db) != 0 ) {
                                CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "WARN: sqlite3_close error!");
                            }

                            if( rc >= 0 ) {
                                /* everything is fine */
                                c_free_locale_string(wstatedb);
                                return 0;
                            }
                            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Integrity check failed!");
                        } else {
                            /* resources need to be freed even when open failed */
                            sqlite3_close(db);
                            CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "database corrupted, removing!");
                        }
                    } else {
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite version mismatch");
                    }
                }
            }
        }
        /* if it comes here, the database is broken and should be recreated. */
        _tunlink(wstatedb);
    }

  c_free_locale_string(wstatedb);

  /* create database */
  rc = sqlite3_open(statedb, &db);
  if (rc == SQLITE_OK) {
    sqlite3_close(db);
    csync_win32_set_file_hidden(statedb, true);
    return 1;
  }
  sqlite3_close(db);
   CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_open failed: %s %s", sqlite3_errmsg(db), statedb);
   return -1;
}

static int _csync_statedb_is_empty(sqlite3 *db) {
  c_strlist_t *result = NULL;
  int rc = 0;

  result = csync_statedb_query(db, "SELECT COUNT(phash) FROM metadata LIMIT 1 OFFSET 0;");
  if (result == NULL) {
    rc = 1;
  }
  c_strlist_destroy(result);

  return rc;
}

#ifndef NDEBUG
static void sqlite_profile( void *x, const char* sql, sqlite3_uint64 time)
{
    (void)x;
    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
              "_SQL_ %s: %llu", sql, time);

}
#endif

int csync_statedb_load(CSYNC *ctx, const char *statedb, sqlite3 **pdb) {
  int rc = -1;
  int check_rc = -1;
  c_strlist_t *result = NULL;
  sqlite3 *db = NULL;

  if( !ctx ) {
      return -1;
  }

  /* csync_statedb_check tries to open the statedb and creates it in case
   * its not there.
   */
  check_rc = _csync_statedb_check(statedb);
  if (check_rc < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: checking csync database failed - bail out.");

    rc = -1;
    goto out;
  }

  /* Open or create the temporary database */
  if (sqlite3_open(statedb, &db) != SQLITE_OK) {
    const char *errmsg= sqlite3_errmsg(ctx->statedb.db);
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: Failed to sqlite3 open statedb - bail out: %s.",
              errmsg ? errmsg : "<no sqlite3 errormsg>");

    rc = -1;
    goto out;
  }

  /* If check_rc == 1 the database is new and empty as a result. */
  if ((check_rc == 1) || _csync_statedb_is_empty(db)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "statedb doesn't exist");
    csync_set_statedb_exists(ctx, 0);
  } else {
    csync_set_statedb_exists(ctx, 1);
  }

  /* optimization for speeding up SQLite */
  result = csync_statedb_query(db, "PRAGMA synchronous = FULL;");
  c_strlist_destroy(result);
  result = csync_statedb_query(db, "PRAGMA case_sensitive_like = ON;");
  c_strlist_destroy(result);

#ifndef NDEBUG
  sqlite3_profile(db, sqlite_profile, 0 );
#endif
  *pdb = db;

  return 0;
out:
  sqlite3_close(db);
  return rc;
}

int csync_statedb_close(CSYNC *ctx) {
  int rc = 0;

  if (!ctx) {
      return -1;
  }

  /* deallocate query resources */
  if( ctx->statedb.by_hash_stmt ) {
      rc = sqlite3_finalize(ctx->statedb.by_hash_stmt);
      ctx->statedb.by_hash_stmt = NULL;
  }

  if( ctx->statedb.by_fileid_stmt ) {
      rc = sqlite3_finalize(ctx->statedb.by_fileid_stmt);
      ctx->statedb.by_fileid_stmt = NULL;
  }

  if( ctx->statedb.by_inode_stmt ) {
      rc = sqlite3_finalize(ctx->statedb.by_inode_stmt);
      ctx->statedb.by_inode_stmt = NULL;
  }

  sqlite3_close(ctx->statedb.db);

  return rc;
}

// This funciton parses a line from the metadata table into the given csync_file_stat
// structure which it is also allocating.
// Note that this function calls laso sqlite3_step to actually get the info from db and
// returns the sqlite return type.
static int _csync_file_stat_from_metadata_table( csync_file_stat_t **st, sqlite3_stmt *stmt )
{
    int rc = SQLITE_ERROR;
    int column_count;
    int len;

    if( ! stmt ) {
       CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Fatal: Statement is NULL.");
       return SQLITE_ERROR;
    }

    column_count = sqlite3_column_count(stmt);

    rc = sqlite3_step(stmt);

    if( rc == SQLITE_ROW ) {
        if(column_count > 7) {
            const char *name;

            /* phash, pathlen, path, inode, uid, gid, mode, modtime */
            len = sqlite3_column_int(stmt, 1);
            *st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
            if (*st == NULL) {
                return SQLITE_NOMEM;
            }
            /* clear the whole structure */
            ZERO_STRUCTP(*st);

            /* The query suceeded so use the phash we pass to the function. */
            (*st)->phash = sqlite3_column_int64(stmt, 0);

            (*st)->pathlen = sqlite3_column_int(stmt, 1);
            name = (const char*) sqlite3_column_text(stmt, 2);
            memcpy((*st)->path, (len ? name : ""), len + 1);
            (*st)->inode = sqlite3_column_int64(stmt,3);
            (*st)->uid = sqlite3_column_int(stmt, 4);
            (*st)->gid = sqlite3_column_int(stmt, 5);
            (*st)->mode = sqlite3_column_int(stmt, 6);
            (*st)->modtime = strtoul((char*)sqlite3_column_text(stmt, 7), NULL, 10);

            if(*st && column_count > 8 ) {
                (*st)->type = sqlite3_column_int(stmt, 8);
            }

            if(column_count > 9 && sqlite3_column_text(stmt, 9)) {
                (*st)->etag = c_strdup( (char*) sqlite3_column_text(stmt, 9) );
            }
            if(column_count > 10 && sqlite3_column_text(stmt,10)) {
                csync_vio_set_file_id((*st)->file_id, (char*) sqlite3_column_text(stmt, 10));
            }
        }
    }
    return rc;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_hash(CSYNC *ctx,
                                                  uint64_t phash)
{
  csync_file_stat_t *st = NULL;
  int rc;

  if( !ctx ) {
      return NULL;
  }

  if( ctx->statedb.by_hash_stmt == NULL ) {
      const char *hash_query = "SELECT * FROM metadata WHERE phash=?1";

      rc = sqlite3_prepare_v2(ctx->statedb.db, hash_query, strlen(hash_query), &ctx->statedb.by_hash_stmt, NULL);
      if( rc != SQLITE_OK ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for hash query.");
          return NULL;
      }
  }

  if( ctx->statedb.by_hash_stmt == NULL ) {
    return NULL;
  }

  sqlite3_bind_int64(ctx->statedb.by_hash_stmt, 1, (long long signed int)phash);

  if( _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_hash_stmt) < 0 ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata!");
  }
  sqlite3_reset(ctx->statedb.by_hash_stmt);

  return st;
}

csync_file_stat_t *csync_statedb_get_stat_by_file_id(CSYNC *ctx,
                                                      const char *file_id ) {
    csync_file_stat_t *st = NULL;
    int rc = 0;

    if (!file_id) {
        return 0;
    }
    if (c_streq(file_id, "")) {
        return 0;
    }

    if( !ctx ) {
        return NULL;
    }

    if( ctx->statedb.by_fileid_stmt == NULL ) {
        const char *query = "SELECT * FROM metadata WHERE fileid=?1";

        rc = sqlite3_prepare_v2(ctx->statedb.db, query, strlen(query), &ctx->statedb.by_fileid_stmt, NULL);
        if( rc != SQLITE_OK ) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for file id query.");
            return NULL;
        }
    }

    /* bind the query value */
    sqlite3_bind_text(ctx->statedb.by_fileid_stmt, 1, file_id, -1, SQLITE_STATIC);

    if( _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_fileid_stmt) < 0 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata!");
    }
    // clear the resources used by the statement.
    sqlite3_reset(ctx->statedb.by_fileid_stmt);

    return st;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_inode(CSYNC *ctx,
                                                  uint64_t inode)
{
  csync_file_stat_t *st = NULL;
  int rc;

  if (!inode) {
      return NULL;
  }

  if( !ctx ) {
      return NULL;
  }

  if( ctx->statedb.by_inode_stmt == NULL ) {
      const char *inode_query = "SELECT * FROM metadata WHERE inode=?1";

      rc = sqlite3_prepare_v2(ctx->statedb.db, inode_query, strlen(inode_query), &ctx->statedb.by_inode_stmt, NULL);
      if( rc != SQLITE_OK ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for inode query.");
          return NULL;
      }
  }

  if( ctx->statedb.by_inode_stmt == NULL ) {
    return NULL;
  }

  sqlite3_bind_int64(ctx->statedb.by_inode_stmt, 1, (long long signed int)inode);

  if( _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_inode_stmt) < 0 ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata by inode!");
  }
  sqlite3_reset(ctx->statedb.by_inode_stmt);

  return st;
}

/* Get the etag. */
char *csync_statedb_get_etag( CSYNC *ctx, uint64_t jHash ) {
    char *ret = NULL;
    csync_file_stat_t *fs = NULL;

    if( !ctx ) {
        return NULL;
    }

    if( ! csync_get_statedb_exists(ctx)) return ret;

    fs = csync_statedb_get_stat_by_hash(ctx, jHash );
    if( fs ) {
        if( fs->etag ) {
            ret = c_strdup(fs->etag);
        }
        csync_file_stat_free(fs);
    }

    return ret;
}

#define BELOW_PATH_QUERY "SELECT phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5, fileid FROM metadata WHERE path LIKE(?)"

int csync_statedb_get_below_path( CSYNC *ctx, const char *path ) {
    int rc;
    sqlite3_stmt *stmt = NULL;
    int64_t cnt = 0;
    char *likepath;
    int asp;

    if( !path ) {
        return -1;
    }

    if( !ctx ) {
        return -1;
    }

    rc = sqlite3_prepare_v2(ctx->statedb.db, BELOW_PATH_QUERY, -1, &stmt, NULL);
    if( rc != SQLITE_OK ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for hash query.");
      return -1;
    }

    if (stmt == NULL) {
      return -1;
    }

    asp = asprintf( &likepath, "%s/%%%%", path);
    if (asp < 0) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "asprintf failed!");
        return -1;
    }

    sqlite3_bind_text(stmt, 1, likepath, -1, SQLITE_STATIC);

    cnt = 0;

    do {
        csync_file_stat_t *st = NULL;

        rc = _csync_file_stat_from_metadata_table( &st, stmt);
        if( st ) {
            /* store into result list. */
            if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
                SAFE_FREE(st);
                ctx->status_code = CSYNC_STATUS_TREE_ERROR;
                break;
            }
            cnt++;
        }
    } while( rc == SQLITE_ROW );

    if( rc != SQLITE_DONE ) {
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
    } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "%ld entries read below path %s from db.", cnt, path);
    }
    sqlite3_finalize(stmt);
    SAFE_FREE(likepath);

    return 0;
}

/* query the statedb, caller must free the memory */
c_strlist_t *csync_statedb_query(sqlite3 *db,
                                 const char *statement) {
  int err = SQLITE_OK;
  int rc = SQLITE_OK;
  size_t i = 0;
  size_t busy_count = 0;
  size_t retry_count = 0;
  size_t column_count = 0;
  sqlite3_stmt *stmt;
  const char *tail = NULL;
  const char *field = NULL;
  c_strlist_t *result = NULL;
  int row = 0;

  do {
    /* compile SQL program into a virtual machine, reattempteing if busy */
    do {
      if (busy_count) {
        /* sleep 100 msec */
        usleep(100000);
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "sqlite3_prepare: BUSY counter: %zu", busy_count);
      }
      err = sqlite3_prepare(db, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count ++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN,
                "sqlite3_compile error: %s - on query %s",
                sqlite3_errmsg(db), statement);
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

       row++;
        if( result ) {
            result = c_strlist_expand(result, row*column_count);
        } else {
            result = c_strlist_new(column_count);
        }

        if (result == NULL) {
          return NULL;
        }

        /* iterate over columns */
        for (i = 0; i < column_count; i++) {
          field = (const char *) sqlite3_column_text(stmt, i);
          if (!field)
              field = "";
          // CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "sqlite3_column_text: %s", field);
          if (c_strlist_add(result, field) < 0) {
            c_strlist_destroy(result);
            return NULL;
          }
        }
      } /* end infinite for loop */

      /* deallocate vm resources */
      rc = sqlite3_finalize(stmt);

      if (err != SQLITE_DONE && rc != SQLITE_SCHEMA) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s - on query: %s", sqlite3_errmsg(db), statement);
        if (result != NULL) {
          c_strlist_destroy(result);
        }
        return NULL;
      }

      if (rc == SQLITE_SCHEMA) {
        retry_count ++;
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "SQLITE_SCHEMA error occurred on query: %s", statement);
        if (retry_count < 10) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Retrying now.");
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "RETRY count has reached its maximum. Aborting statement: %s", statement);
          if (result != NULL) {
            c_strlist_destroy(result);
          }
          result = c_strlist_new(1);
        }
      }
    }
  } while (rc == SQLITE_SCHEMA && retry_count < 10);

  return result;
}

/* vim: set ts=8 sw=2 et cindent: */
