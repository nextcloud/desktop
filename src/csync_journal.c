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
 *
 * vim: ts=2 sw=2 et cindent
 */

#include <sqlite3.h>
#include <unistd.h>

#include "std/c_string.h"
#include "csync_private.h"
#include "csync_journal.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.journal"
#include "csync_log.h"

static int csync_journal_check(const char *journal) {
  FILE *fp = NULL;
  char buf[16] = {0};
  sqlite3 *db = NULL;

  /* check db version */
  fp = fopen(journal, "r");
  if (fp) {
    if (fread((void *) buf, (size_t) 16, 1, fp) == 0) {
      buf[16] = '\0';
      fclose(fp);
      if (c_streq(buf, "SQLite format 3")) {
        if (sqlite3_open(journal, &db ) == SQLITE_OK) {
          /* everything is fine */
          sqlite3_close(db);
          return 0;
        } else {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "database corrupted, removing!");
          unlink(journal);
        }
      } else {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite version mismatch");
        unlink(journal);
      }
    }
  }

  /* create database */
  if (sqlite3_open(journal, &db ) == SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  return -1;
}

static int csync_journal_is_empty(CSYNC *ctx) {
  c_strlist_t *result = NULL;
  int rc = 0;

  result = csync_journal_query(ctx, "SELECT COUNT(key) FROM metadata LIMIT 1 OFFSET 0;");
  if (result && result->count == 0) {
    rc = 1;
  }
  c_strlist_destroy(result);

  return rc;
}

int csync_journal_load(CSYNC *ctx, const char *journal) {
  if (csync_journal_check(journal) < 0) {
    return -1;
  }

  if (sqlite3_open(journal, &ctx->internal->_journal) != SQLITE_OK) {
    return -1;
  }

  if (csync_journal_is_empty(ctx)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "Journal doesn't exist");
    ctx->internal->_journal_exists = 0;
  } else {
    ctx->internal->_journal_exists = 1;
  }

  return 0;
}

/* TODO: void csync_journal_create_tables(CSYNC *ctx) */
/* TODO: void csync_journal_empty_tables(CSYNC *ctx) */

c_strlist_t *csync_journal_query(CSYNC *ctx, const char *statement) {
  int err;
  int rc = 0;
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "sqlite3_prepare: BUSY counter: %d", busy_count);
      }
      err = sqlite3_prepare(ctx->internal->_journal, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count ++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "sqlite3_compile error: %s - on query %s", sqlite3_errmsg(ctx->internal->_journal), statement);
      result = c_strlist_new(1);
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
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "sqlite3_step: BUSY counter: %d", busy_count);
          continue;
        }

        if (err == SQLITE_MISUSE) {
          CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_step: MISUSE!!");
        }

        if (err == SQLITE_DONE || err == SQLITE_ERROR) {
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s - on query: %s", sqlite3_errmsg(ctx->internal->_journal), statement);
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

int csync_journal_insert(CSYNC *ctx, const char *statement) {
  int err;
  int rc = 0;
  int busy_count = 0;
  int retry_count = 0;
  sqlite3_stmt *stmt;
  const char *tail;

  do {
    /* compile SQL program into a virtual machine, reattempteing if busy */
    do {
      if (busy_count) {
        /* sleep 100 msec */
        usleep(100000);
        CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "sqlite3_prepare: BUSY counter: %d", busy_count);
      }
      err = sqlite3_prepare(ctx->internal->_journal, statement, -1, &stmt, &tail);
    } while (err == SQLITE_BUSY && busy_count++ < 120);

    if (err != SQLITE_OK) {
      if (err == SQLITE_BUSY) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Gave up waiting for lock to clear");
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite3_compile error: %s on query %s", sqlite3_errmsg(ctx->internal->_journal), statement);
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
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "sqlite_step error: %s on insert: %s", sqlite3_errmsg(ctx->internal->_journal), statement);
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

  return sqlite3_last_insert_rowid(ctx->internal->_journal);
}

