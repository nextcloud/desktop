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
#include <inttypes.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_statedb.h"
#include "csync_util.h"
#include "csync_misc.h"
#include "csync_exclude.h"

#include "c_string.h"
#include "c_jhash.h"
#include "csync_time.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.statedb"
#include "csync_log.h"
#include "csync_rename.h"

#define BUF_SIZE 16

#define sqlite_open(A, B) sqlite3_open_v2(A,B, SQLITE_OPEN_READONLY+SQLITE_OPEN_NOMUTEX, NULL)

#define SQLTM_TIME 150000
#define SQLTM_COUNT 10

#define SQLITE_BUSY_HANDLED(F) if(1) { \
    int n = 0; \
    do { rc = F ; \
      if( (rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED) ) { \
         n++; \
         usleep(SQLTM_TIME); \
      } \
    }while( (n < SQLTM_COUNT) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED))); \
  }


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

    if( sqlite3_threadsafe() == 0 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "* WARNING: SQLite module is not threadsafe!");
        rc = -1;
    }

    return rc;
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
  c_strlist_t *result = NULL;
  sqlite3 *db = NULL;

  if( !ctx ) {
      return -1;
  }

  if (ctx->statedb.db) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: DB already open");
      ctx->status_code = CSYNC_STATUS_PARAM_ERROR;
      return -1;
  }

  ctx->statedb.lastReturnValue = SQLITE_OK;

  /* Openthe database */
  if (sqlite_open(statedb, &db) != SQLITE_OK) {
    const char *errmsg= sqlite3_errmsg(ctx->statedb.db);
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: Failed to sqlite3 open statedb - bail out: %s.",
              errmsg ? errmsg : "<no sqlite3 errormsg>");

    rc = -1;
    ctx->status_code = CSYNC_STATUS_STATEDB_LOAD_ERROR;
    goto out;
  }

  if (_csync_check_db_integrity(db) != 0) {
      const char *errmsg= sqlite3_errmsg(db);
      CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: sqlite3 integrity check failed - bail out: %s.",
                errmsg ? errmsg : "<no sqlite3 errormsg>");
      rc = -1;
      ctx->status_code = CSYNC_STATUS_STATEDB_CORRUPTED;
      goto out;
  }

  if (_csync_statedb_is_empty(db)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "statedb contents doesn't exist");
    csync_set_statedb_exists(ctx, 0);
  } else {
    csync_set_statedb_exists(ctx, 1);
  }

  /* Print out the version */
  //
  result = csync_statedb_query(db, "SELECT sqlite_version();");
  if (result && result->count >= 1) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "sqlite3 version \"%s\"", *result->vector);
  }
  c_strlist_destroy(result);

  /* optimization for speeding up SQLite */
  result = csync_statedb_query(db, "PRAGMA synchronous = NORMAL;");
  c_strlist_destroy(result);
  result = csync_statedb_query(db, "PRAGMA case_sensitive_like = ON;");
  c_strlist_destroy(result);

  /* set a busy handler with 5 seconds timeout */
  sqlite3_busy_timeout(db, 5000);

#ifndef NDEBUG
  sqlite3_profile(db, sqlite_profile, 0 );
#endif
  *pdb = db;

   CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "Success");

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
  if( ctx->statedb.by_fileid_stmt ) {
      sqlite3_finalize(ctx->statedb.by_fileid_stmt);
      ctx->statedb.by_fileid_stmt = NULL;
  }
  if( ctx->statedb.by_hash_stmt ) {
      sqlite3_finalize(ctx->statedb.by_hash_stmt);
      ctx->statedb.by_hash_stmt = NULL;
  }
  if( ctx->statedb.by_inode_stmt) {
      sqlite3_finalize(ctx->statedb.by_inode_stmt);
      ctx->statedb.by_inode_stmt = NULL;
  }

  ctx->statedb.lastReturnValue = SQLITE_OK;

  int sr = sqlite3_close(ctx->statedb.db);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "sqlite3_close=%d", sr);

  ctx->statedb.db = 0;

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

    SQLITE_BUSY_HANDLED( sqlite3_step(stmt) );

    if( rc == SQLITE_ROW ) {
        if(column_count > 7) {
            const char *name;

            /* phash, pathlen, path, inode, uid, gid, mode, modtime */
            len = sqlite3_column_int(stmt, 1);
            *st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
            /* clear the whole structure */
            ZERO_STRUCTP(*st);

            /* The query suceeded so use the phash we pass to the function. */
            (*st)->phash = sqlite3_column_int64(stmt, 0);

            (*st)->pathlen = sqlite3_column_int(stmt, 1);
            name = (const char*) sqlite3_column_text(stmt, 2);
            memcpy((*st)->path, (len ? name : ""), len + 1);
            (*st)->inode = sqlite3_column_int64(stmt,3);
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
            if(column_count > 11 && sqlite3_column_text(stmt,11)) {
                strncpy((*st)->remotePerm,
                        (char*) sqlite3_column_text(stmt, 11),
                        REMOTE_PERM_BUF_SIZE);
            }
            if(column_count > 12 && sqlite3_column_int64(stmt,12)) {
                (*st)->size = sqlite3_column_int64(stmt, 12);
            }
        }
    } else {
        if( rc != SQLITE_DONE ) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "WARN: Query results in %d", rc);
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

  if( !ctx || ctx->db_is_empty ) {
      return NULL;
  }

  if( ctx->statedb.by_hash_stmt == NULL ) {
      const char *hash_query = "SELECT * FROM metadata WHERE phash=?1";

      SQLITE_BUSY_HANDLED(sqlite3_prepare_v2(ctx->statedb.db, hash_query, strlen(hash_query), &ctx->statedb.by_hash_stmt, NULL));
      ctx->statedb.lastReturnValue = rc;
      if( rc != SQLITE_OK ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for hash query.");
          return NULL;
      }
  }

  if( ctx->statedb.by_hash_stmt == NULL ) {
    return NULL;
  }

  sqlite3_bind_int64(ctx->statedb.by_hash_stmt, 1, (long long signed int)phash);

  rc = _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_hash_stmt);
  ctx->statedb.lastReturnValue = rc;
  if( !(rc == SQLITE_ROW || rc == SQLITE_DONE) )  {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata: %d!", rc);
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

    if( !ctx || ctx->db_is_empty ) {
        return NULL;
    }

    if( ctx->statedb.by_fileid_stmt == NULL ) {
        const char *query = "SELECT * FROM metadata WHERE fileid=?1";

        SQLITE_BUSY_HANDLED(sqlite3_prepare_v2(ctx->statedb.db, query, strlen(query), &ctx->statedb.by_fileid_stmt, NULL));
        ctx->statedb.lastReturnValue = rc;
        if( rc != SQLITE_OK ) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for file id query.");
            return NULL;
        }
    }

    /* bind the query value */
    sqlite3_bind_text(ctx->statedb.by_fileid_stmt, 1, file_id, -1, SQLITE_STATIC);

    rc = _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_fileid_stmt);
    ctx->statedb.lastReturnValue = rc;
    if( !(rc == SQLITE_ROW || rc == SQLITE_DONE) ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata: %d!", rc);
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

  if( !ctx || ctx->db_is_empty ) {
      return NULL;
  }

  if( ctx->statedb.by_inode_stmt == NULL ) {
      const char *inode_query = "SELECT * FROM metadata WHERE inode=?1";

      SQLITE_BUSY_HANDLED(sqlite3_prepare_v2(ctx->statedb.db, inode_query, strlen(inode_query), &ctx->statedb.by_inode_stmt, NULL));
      ctx->statedb.lastReturnValue = rc;
      if( rc != SQLITE_OK ) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for inode query.");
          return NULL;
      }
  }

  if( ctx->statedb.by_inode_stmt == NULL ) {
    return NULL;
  }

  sqlite3_bind_int64(ctx->statedb.by_inode_stmt, 1, (long long signed int)inode);

  rc = _csync_file_stat_from_metadata_table(&st, ctx->statedb.by_inode_stmt);
  ctx->statedb.lastReturnValue = rc;
  if( !(rc == SQLITE_ROW || rc == SQLITE_DONE) ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Could not get line from metadata by inode: %d!", rc);
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

#define BELOW_PATH_QUERY "SELECT phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5, fileid, remotePerm, filesize FROM metadata WHERE pathlen>? AND path LIKE(?)"

int csync_statedb_get_below_path( CSYNC *ctx, const char *path ) {
    int rc;
    sqlite3_stmt *stmt = NULL;
    int64_t cnt = 0;
    char *likepath;
    int asp;
    int min_path_len;

    if( !path ) {
        return -1;
    }

    if( !ctx || ctx->db_is_empty ) {
        return -1;
    }

    SQLITE_BUSY_HANDLED(sqlite3_prepare_v2(ctx->statedb.db, BELOW_PATH_QUERY, -1, &stmt, NULL));
    ctx->statedb.lastReturnValue = rc;
    if( rc != SQLITE_OK ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for below path query.");
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

    min_path_len = strlen(path);
    sqlite3_bind_int(stmt, 1, min_path_len);
    sqlite3_bind_text(stmt, 2, likepath, -1, SQLITE_STATIC);

    cnt = 0;

    ctx->statedb.lastReturnValue = rc;
    do {
        csync_file_stat_t *st = NULL;

        rc = _csync_file_stat_from_metadata_table( &st, stmt);
        if( st ) {
            /* Check for exclusion from the tree.
             * Note that this is only a safety net in case the ignore list changes
             * without a full remote discovery being triggered. */
            CSYNC_EXCLUDE_TYPE excluded = csync_excluded(ctx, st->path, st->type);
            if (excluded != CSYNC_NOT_EXCLUDED) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "%s excluded (%d)", st->path, excluded);

                if (excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE
                        || excluded == CSYNC_FILE_SILENTLY_EXCLUDED) {
                    SAFE_FREE(st);
                    continue;
                }

                st->instruction = CSYNC_INSTRUCTION_IGNORE;
            }

            /* store into result list. */
            if (c_rbtree_insert(ctx->remote.tree, (void *) st) < 0) {
                SAFE_FREE(st);
                ctx->status_code = CSYNC_STATUS_TREE_ERROR;
                break;
            }
            cnt++;
        }
    } while( rc == SQLITE_ROW );

    ctx->statedb.lastReturnValue = rc;
    if( rc != SQLITE_DONE ) {
        ctx->status_code = CSYNC_STATUS_TREE_ERROR;
    } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "%" PRId64 " entries read below path %s from db.", cnt, path);
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
