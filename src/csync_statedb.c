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
#include "csync_time.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.statedb"
#include "csync_log.h"

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
    r = read(fd, (void *) buf, sizeof(buf) - 1);
    close(fd);
    if (r >= 0) {
      buf[BUF_SIZE - 1] = '\0';
      if (c_streq(buf, "SQLite format 3")) {
        if (sqlite3_open(statedb, &db ) == SQLITE_OK) {
          rc = _csync_check_db_integrity(db);

          sqlite3_close(db);

          if(rc >= 0) {
            /* everything is fine */
            c_free_locale_string(wstatedb);
            return 0;
          }
        } else {
          /* resources need to be freed even when open failed */
          sqlite3_close(db);
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "database corrupted, removing!");
        }
      } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite version mismatch");
      }
    }
    _tunlink(wstatedb);
  }

  c_free_locale_string(wstatedb);
 
  /* create database */
  rc = sqlite3_open(statedb, &db);
  if (rc == SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "%s %s", sqlite3_errmsg(db), statedb);
  sqlite3_close(db);

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
  c_strlist_t *result = NULL;
  char *statedb_tmp = NULL;
  sqlite3 *db = NULL;

  rc = _csync_statedb_check(statedb);
  if (rc < 0) {
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
    rc = -1;
    goto out;
  }

  rc = c_copy(statedb, statedb_tmp, 0644);
  if (rc < 0) {
    goto out;
  }

  /* Open the temporary database */
  if (sqlite3_open(statedb_tmp, &db) != SQLITE_OK) {
    rc = -1;
    goto out;
  }
  SAFE_FREE(statedb_tmp);

  if (_csync_statedb_is_empty(db)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "statedb doesn't exist");
    csync_set_statedb_exists(ctx, 0);
  } else {
    csync_set_statedb_exists(ctx, 1);
  }

  /* optimization for speeding up SQLite */
  result = csync_statedb_query(db, "PRAGMA default_synchronous = FULL;");
  c_strlist_destroy(result);

  *pdb = db;

  return 0;
out:
  sqlite3_close(db);
  SAFE_FREE(statedb_tmp);
  return rc;
}

int csync_statedb_write(CSYNC *ctx, sqlite3 *db)
{
  struct timespec start, finish;
  int rc;

  csync_gettime(&start);
  /* drop tables */
  rc = csync_statedb_drop_tables(db);
  if (rc < 0) {
    return -1;
  }
  csync_gettime(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "## DROPPING tables took %.2f seconds",
                c_secdiff(finish, start));

  /* create tables */
  csync_gettime(&start);
  rc = csync_statedb_create_tables(db);
  if (rc < 0) {
    return -1;
  }
  csync_gettime(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "## CREATE tables took %.2f seconds",
                c_secdiff(finish, start));

  /* insert metadata */
  csync_gettime(&start);
  rc = csync_statedb_insert_metadata(ctx, db);
  if (rc < 0) {
    return -1;
  }
  csync_gettime(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "## INSERT took %.2f seconds",
                c_secdiff(finish, start));

  return 0;
}

int csync_statedb_close(const char *statedb, sqlite3 *db, int jwritten) {
  char *statedb_tmp = NULL;
  int rc = 0;
  mbchar_t *mb_statedb = NULL;

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
      if (_csync_statedb_check(statedb_tmp) == 0) {
          /* New statedb is valid. */
          mb_statedb = c_utf8_to_locale(statedb);

          /* Move the tmp-db to the real one. */
          if (c_rename(statedb_tmp, statedb) < 0) {
              CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                        "Renaming tmp db to original db failed.");
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
  SAFE_FREE(statedb_tmp);

  return rc;
}

int csync_statedb_create_tables(sqlite3 *db) {
  c_strlist_t *result = NULL;

  /*
   * Create temorary table to work on, this speeds up the
   * creation of the statedb if we later just rename it to its
   * final name metadata
   */
  result = csync_statedb_query(db,
      "CREATE TABLE IF NOT EXISTS metadata_temp("
      "phash INTEGER(8),"
      "pathlen INTEGER,"
      "path VARCHAR(4096),"
      "inode INTEGER,"
      "uid INTEGER,"
      "gid INTEGER,"
      "mode INTEGER,"
      "modtime INTEGER(8),"
      "PRIMARY KEY(phash)"
      ");"
      );

  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  /*
   * Create the 'real' table in case it does not exist. This is important
   * for first time sync. Otherwise other functions that query metadata
   * table whine about the table not existing.
   */
  result = csync_statedb_query(db,
      "CREATE TABLE IF NOT EXISTS metadata("
      "phash INTEGER(8),"
      "pathlen INTEGER,"
      "path VARCHAR(4096),"
      "inode INTEGER,"
      "uid INTEGER,"
      "gid INTEGER,"
      "mode INTEGER,"
      "modtime INTEGER(8),"
      "PRIMARY KEY(phash)"
      ");"
      );

  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);


  return 0;
}

int csync_statedb_drop_tables(sqlite3* db) {
  c_strlist_t *result = NULL;

  result = csync_statedb_query(db,
      "DROP TABLE IF EXISTS metadata_temp;"
      );
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  return 0;
}

static int _insert_metadata_visitor(void *obj, void *data) {
  csync_file_stat_t *fs = NULL;
  int rc = -1;
  sqlite3_stmt* stmt;

  fs = (csync_file_stat_t *) obj;
  stmt = (sqlite3_stmt *) data;
  if (stmt == NULL) {
    return -1;
  }

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
              "\t\t\t(phash, pathlen, path, inode, uid, gid, mode, modtime) VALUES \n"
              "\t\t\t(%llu, %lu, %s, %llu, %u, %u, %u, %lu);",
              (long long unsigned int) fs->phash,
              (long unsigned int) fs->pathlen,
              fs->path,
              (long long unsigned int) fs->inode,
              fs->uid,
              fs->gid,
              fs->mode,
              fs->modtime);

    /*
       * The phash needs to be long long unsigned int or it segfaults on PPC
       */
    sqlite3_bind_int64(stmt, 1, (long long signed int) fs->phash);
    sqlite3_bind_int64(stmt, 2, (long unsigned int) fs->pathlen);
    sqlite3_bind_text( stmt, 3, fs->path, fs->pathlen, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, (long long signed int) fs->inode);
    sqlite3_bind_int(  stmt, 5, fs->uid);
    sqlite3_bind_int(  stmt, 6, fs->gid);
    sqlite3_bind_int(  stmt, 7, fs->mode);
    sqlite3_bind_int64(stmt, 8, fs->modtime);

    rc = 0;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite insert failed!");
      rc = -1;
    }

    sqlite3_reset(stmt);

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

int csync_statedb_insert_metadata(CSYNC *ctx, sqlite3 *db) {
  c_strlist_t *result = NULL;
  struct timespec start, step1, step2, finish;
  int rc;

  char buffer[] = "INSERT INTO metadata_temp VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)";
  sqlite3_stmt* stmt;

  csync_gettime(&start);

  /* prepare the INSERT statement */
  rc = sqlite3_prepare_v2(db, buffer, strlen(buffer), &stmt, NULL);
  if (rc != SQLITE_OK) {
    return -1;
  }

  /* Use transactions as that really speeds up processing */
  result = csync_statedb_query(db, "BEGIN TRANSACTION;");
  c_strlist_destroy(result);

  if (c_rbtree_walk(ctx->local.tree, stmt, _insert_metadata_visitor) < 0) {
    /* inserting failed. Drop the metadata_temp table. */
    result = csync_statedb_query(db, "ROLLBACK TRANSACTION;");
    c_strlist_destroy(result);

    result = csync_statedb_query(db, "DROP TABLE IF EXISTS metadata_temp;");
    c_strlist_destroy(result);

    return -1;
  }

  result = csync_statedb_query(db, "COMMIT TRANSACTION;");
  c_strlist_destroy(result);

  sqlite3_finalize(stmt);
  csync_gettime(&step1);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "Transaction1 took %.2f seconds",
                 c_secdiff(step1, start));

  /* Drop table metadata */
  result = csync_statedb_query(db, "BEGIN TRANSACTION;");
  c_strlist_destroy(result);

  result = csync_statedb_query(db, "DROP TABLE IF EXISTS metadata;");
  c_strlist_destroy(result);

  /* Rename temp table to real table. */
  result = csync_statedb_query(db, "ALTER TABLE metadata_temp RENAME TO metadata;");
  c_strlist_destroy(result);

  result = csync_statedb_query(db, "COMMIT TRANSACTION;");
  c_strlist_destroy(result);

  csync_gettime(&step2);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "ALTERING tables took %.2f seconds",
                c_secdiff(step2, step1));

  /* Recreate indices */
  result = csync_statedb_query(db, "BEGIN TRANSACTION;");
  c_strlist_destroy(result);

  result = csync_statedb_query(db,
      "CREATE INDEX IF NOT EXISTS metadata_phash ON metadata(phash);");
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(db,
      "CREATE INDEX IF NOT EXISTS metadata_inode ON metadata(inode);");
  if (result == NULL) {
    return -1;
  }
  c_strlist_destroy(result);

  result = csync_statedb_query(db, "COMMIT TRANSACTION;");
  c_strlist_destroy(result);

  csync_gettime(&finish);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                "INDEXING took %.2f seconds",
                c_secdiff(finish, step2));


  return 0;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_hash(sqlite3 *db,
                                                  uint64_t phash)
{
  csync_file_stat_t *st = NULL;
  c_strlist_t *result = NULL;
  char *stmt = NULL;
  size_t len = 0;

  /* Use %lld instead of %llu otherwise there is an overflow in sqlite
   * which only supports signed */
  stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE phash='%lld'",
      (long long int) phash);
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

  c_strlist_destroy(result);

  return st;
}

/* caller must free the memory */
csync_file_stat_t *csync_statedb_get_stat_by_inode(sqlite3 *db,
                                                   ino_t inode) {
  csync_file_stat_t *st = NULL;
  c_strlist_t *result = NULL;
  char *stmt = NULL;
  size_t len = 0;

#ifdef _WIN32
  /* no idea about inodes. */
  return st;
#endif

  stmt = sqlite3_mprintf("SELECT * FROM metadata WHERE inode='%llu'",
                         (long long unsigned int)inode);
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

  st->phash = strtoull(result->vector[0], NULL, 10);
  st->pathlen = atoi(result->vector[1]);
  memcpy(st->path, (len ? result->vector[2] : ""), len + 1);
  st->inode = atoi(result->vector[3]);
  st->uid = atoi(result->vector[4]);
  st->gid = atoi(result->vector[5]);
  st->mode = atoi(result->vector[6]);
  st->modtime = strtoul(result->vector[7], NULL, 10);

  c_strlist_destroy(result);

  return st;
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
  c_strlist_t *result = NULL;

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

        /* If something went wrong make sure we don't leak memory */
        if (result != NULL) {
          c_strlist_destroy(result);
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s - on query: %s", sqlite3_errmsg(db), statement);
        if (result != NULL) {
          c_strlist_destroy(result);
        }
        result = c_strlist_new(1);
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

int csync_statedb_insert(sqlite3 *db, const char *statement) {
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
      err = sqlite3_prepare(db, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_compile error: %s on query %s", sqlite3_errmsg(db), statement);
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
                  "sqlite_step error: %s on insert: %s",
                  sqlite3_errmsg(db), statement);
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

  return sqlite3_last_insert_rowid(db);
}

/* vim: set ts=8 sw=2 et cindent: */
