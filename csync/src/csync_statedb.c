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
#define HASH_QUERY "SELECT * FROM metadata WHERE phash=?1"

static sqlite3_stmt* _by_hash_stmt = NULL;

void csync_set_statedb_exists(CSYNC *ctx, int val) {
  ctx->statedb.exists = val;
}

int csync_get_statedb_exists(CSYNC *ctx) {
  return ctx->statedb.exists;
}

/* Set the hide attribute in win32. That makes it invisible in normal explorers */
static void _csync_win32_hide_file( const char *file ) {
#ifdef _WIN32
  mbchar_t *fileName;
  DWORD dwAttrs;

  if( !file ) return;

  fileName = c_utf8_to_locale( file );
  dwAttrs = GetFileAttributesW(fileName);

  if (dwAttrs==INVALID_FILE_ATTRIBUTES) return;

  if (!(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
     SetFileAttributesW(fileName, dwAttrs | FILE_ATTRIBUTE_HIDDEN );
  }

  c_free_locale_string(fileName);
#else
    (void) file;
#endif
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
    _csync_win32_hide_file(statedb);
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

int csync_statedb_load(CSYNC *ctx, const char *statedb, sqlite3 **pdb) {
  int rc = -1;
  int check_rc = -1;
  c_strlist_t *result = NULL;
  char *statedb_tmp = NULL;
  sqlite3 *db = NULL;

  /* csync_statedb_check tries to open the statedb and creates it in case
   * its not there.
   */
  check_rc = _csync_statedb_check(statedb);
  if (check_rc < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: checking csync database failed - bail out.");

    rc = -1;
    goto out;
  }

  /*
   * We want a two phase commit for the jounal, so we create a temporary copy
   * of the database.
   * The intention is that if something goes wrong we will not loose the
   * statedb.
   */
  rc = asprintf(&statedb_tmp, "%s.ctmp", statedb);
  if (rc < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: could not create statedb name - bail out.");
    rc = -1;
    goto out;
  }

  if (c_copy(statedb, statedb_tmp, 0644) < 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: Failed to copy statedb -> statedb_tmp - bail out.");

    rc = -1;
    goto out;
  }

  _csync_win32_hide_file( statedb_tmp );

  /* Open or create the temporary database */
  if (sqlite3_open(statedb_tmp, &db) != SQLITE_OK) {
    const char *errmsg= sqlite3_errmsg(ctx->statedb.db);
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "ERR: Failed to sqlite3 open statedb - bail out: %s.",
              errmsg ? errmsg : "<no sqlite3 errormsg>");

    rc = -1;
    goto out;
  }
  SAFE_FREE(statedb_tmp);

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

  *pdb = db;

  return 0;
out:
  sqlite3_close(db);
  SAFE_FREE(statedb_tmp);
  return rc;
}

int csync_statedb_close(const char *statedb, sqlite3 *db, int jwritten) {
  char *statedb_tmp = NULL;
  mbchar_t* wstatedb_tmp = NULL;
  int rc = 0;

  mbchar_t *mb_statedb = NULL;

  /* deallocate query resources */
  rc = sqlite3_finalize(_by_hash_stmt);
  _by_hash_stmt = NULL;

  /* close the temporary database */
  sqlite3_close(db);

  if (asprintf(&statedb_tmp, "%s.ctmp", statedb) < 0) {
    return -1;
  }

  /* If we successfully synchronized, overwrite the original statedb */

  /*
   * Check the integrity of the tmp db. If ok, overwrite the old database with
   * the tmp db.
   */
  if (jwritten) {
      /* statedb check returns either
       * 0  : database exists and is fine
       * 1  : new database was set up
       * -1 : error.
       */
      if (_csync_statedb_check(statedb_tmp) >= 0) {
          /* New statedb is valid. */
          mb_statedb = c_utf8_to_locale(statedb);

          /* Move the tmp-db to the real one. */
          if (c_rename(statedb_tmp, statedb) < 0) {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                        "Renaming tmp db to original db failed. (errno=%d)", errno);
              rc = -1;
          } else {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                        "Successfully moved tmp db to original db.");
          }
      } else {
          mb_statedb = c_utf8_to_locale(statedb_tmp);
          _tunlink(mb_statedb);

          /* new statedb_tmp is not integer. */
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "  ## csync tmp statedb corrupt. Original one is not replaced. ");
          rc = -1;
      }
      c_free_locale_string(mb_statedb);
  }

  wstatedb_tmp = c_utf8_to_locale(statedb_tmp);
  if (wstatedb_tmp) {
      _tunlink(wstatedb_tmp);
      c_free_locale_string(wstatedb_tmp);
  }

  SAFE_FREE(statedb_tmp);

  return rc;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_hash(sqlite3 *db,
                                                  uint64_t phash)
{
  csync_file_stat_t *st = NULL;
  size_t len = 0;
  int column_count = 0;
  int rc;

  if( _by_hash_stmt == NULL ) {
    rc = sqlite3_prepare_v2(db, HASH_QUERY, strlen(HASH_QUERY), &_by_hash_stmt, NULL);
    if( rc != SQLITE_OK ) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "WRN: Unable to create stmt for hash query.");
      return NULL;
    }
  }

  if( _by_hash_stmt == NULL ) {
    return NULL;
  }

  column_count = sqlite3_column_count(_by_hash_stmt);

  sqlite3_bind_int64(_by_hash_stmt, 1, (long long signed int)phash);
  rc = sqlite3_step(_by_hash_stmt);

  if( rc == SQLITE_ROW ) {
    if(column_count > 7) {
      /* phash, pathlen, path, inode, uid, gid, mode, modtime */
      len = sqlite3_column_int(_by_hash_stmt, 1);
      st = c_malloc(sizeof(csync_file_stat_t) + len + 1);
      if (st == NULL) {
        return NULL;
      }
      /* clear the whole structure */
      ZERO_STRUCTP(st);

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

      st->pathlen = sqlite3_column_int(_by_hash_stmt, 1);
      memcpy(st->path, (len ? (char*) sqlite3_column_text(_by_hash_stmt, 2) : ""), len + 1);
      st->inode = sqlite3_column_int64(_by_hash_stmt,3);
      st->uid = sqlite3_column_int(_by_hash_stmt, 4);
      st->gid = sqlite3_column_int(_by_hash_stmt, 5);
      st->mode = sqlite3_column_int(_by_hash_stmt, 6);
      st->modtime = strtoul((char*)sqlite3_column_text(_by_hash_stmt, 7), NULL, 10);

      if(st && column_count > 8 ) {
        st->type = sqlite3_column_int(_by_hash_stmt, 8);
      }

      if(column_count > 9 && sqlite3_column_text(_by_hash_stmt, 9)) {
        st->etag = c_strdup( (char*) sqlite3_column_text(_by_hash_stmt, 9) );
      }
      if(column_count > 10 && sqlite3_column_text(_by_hash_stmt,10)) {
          csync_vio_set_file_id(st->file_id, (char*) sqlite3_column_text(_by_hash_stmt, 10));
      }
    }
  } else {
    /* SQLITE_DONE says there is no further row. That's not an error. */
    if (rc != SQLITE_DONE) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite hash query fail: %s", sqlite3_errmsg(db));
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "No result record found for phash = %llu",
              (long long unsigned int) phash);
    SAFE_FREE(st);
  }

  sqlite3_reset(_by_hash_stmt);

  return st;
}

csync_file_stat_t *csync_statedb_get_stat_by_file_id( sqlite3 *db,
                                                     const char *file_id ) {
   csync_file_stat_t *st = NULL;
   c_strlist_t *result = NULL;
   char *stmt = NULL;
   size_t len = 0;

   if (!file_id) {
       return 0;
   }
   if (c_streq(file_id, "")) {
       return 0;
   }
   stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE fileid='%q'",
                          file_id);

   if (stmt == NULL) {
     return NULL;
   }

   result = csync_statedb_query(db, stmt);
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
   /* clear the whole structure */
   ZERO_STRUCTP(st);

   st->phash    = atoll(result->vector[0]);
   st->pathlen  = atoi(result->vector[1]);
   memcpy(st->path, (len ? result->vector[2] : ""), len + 1);
   st->inode    = atoll(result->vector[3]);
   st->uid      = atoi(result->vector[4]);
   st->gid      = atoi(result->vector[5]);
   st->mode     = atoi(result->vector[6]);
   st->modtime  = strtoul(result->vector[7], NULL, 10);
   st->type     = atoi(result->vector[8]);
   if( result->vector[9] )
     st->etag = c_strdup(result->vector[9]);

   csync_vio_set_file_id(st->file_id, file_id);

   c_strlist_destroy(result);

   return st;
 }


/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_inode(sqlite3 *db,
                                                   uint64_t inode) {
  csync_file_stat_t *st = NULL;
  c_strlist_t *result = NULL;
  char *stmt = NULL;
  size_t len = 0;

  if (!inode) {
      return NULL;
  }

  stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE inode='%lld'",
             (long long signed int) inode);
  if (stmt == NULL) {
    return NULL;
  }

  result = csync_statedb_query(db, stmt);
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
  /* clear the whole structure */
  ZERO_STRUCTP(st);

  st->phash = atoll(result->vector[0]);
  st->pathlen = atoi(result->vector[1]);
  memcpy(st->path, (len ? result->vector[2] : ""), len + 1);
  st->inode = atoll(result->vector[3]);
  st->uid = atoi(result->vector[4]);
  st->gid = atoi(result->vector[5]);
  st->mode = atoi(result->vector[6]);
  st->modtime = strtoul(result->vector[7], NULL, 10);
  st->type = atoi(result->vector[8]);
  if( result->vector[9] )
    st->etag = c_strdup(result->vector[9]);
  csync_vio_set_file_id( st->file_id, result->vector[10]);

  c_strlist_destroy(result);

  return st;
}

/* Get the etag.  (it is called unique id for legacy reason
 * and it is the field md5 in the database for legacy reason */
char *csync_statedb_get_uniqId( CSYNC *ctx, uint64_t jHash, csync_vio_file_stat_t *buf ) {
    char *ret = NULL;
    c_strlist_t *result = NULL;
    char *stmt = NULL;
    (void)buf;

    if( ! csync_get_statedb_exists(ctx)) return ret;

    stmt = sqlite3_mprintf("SELECT md5, fileid FROM metadata WHERE phash='%lld'", jHash);

    result = csync_statedb_query(ctx->statedb.db, stmt);
    sqlite3_free(stmt);
    if (result == NULL) {
      return NULL;
    }

    if (result->count == 2) {
        ret = c_strdup( result->vector[0] );
        csync_vio_file_stat_set_file_id(buf, result->vector[1]);
    }

    c_strlist_destroy(result);

    return ret;
}

c_strlist_t *csync_statedb_get_below_path( CSYNC *ctx, const char *path ) {
    c_strlist_t *list = NULL;
    char *stmt = NULL;

    stmt = sqlite3_mprintf("SELECT phash, path, inode, uid, gid, mode, modtime, type, md5, fileid "
                           "FROM metadata WHERE path LIKE('%q/%%')", path);
    if (stmt == NULL) {
      return NULL;
    }

    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "SQL: %s", stmt);

    list = csync_statedb_query( ctx->statedb.db, stmt );

    sqlite3_free(stmt);

    return list;
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
